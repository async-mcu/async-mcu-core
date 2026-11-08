# auxiliary — стенд автономной проверки Pin-примеров

Второй МК (ESP32-C3, env `test_stimulus`) подаёт тестовые сигналы на пины DUT
(классический ESP32), пока тот бодрствует или спит. позволяет проверять
прерывания без участия человека: меандры, одиночные импульсы, эмуляция дребезга.

## Состав

| Что | Где |
|---|---|
| Прошивка стимулятора (голый Arduino, без библиотеки) | `Stimulus/main.cpp`, env `test_stimulus` |
| Пробник уровней пинов (диагностика проводки) | `PinProbe/main.cpp`, env `-aux_pinprobe` |
| Хост: команды стимулятору | `../tools/stimctl.py` |
| Хост: чтение COM DUT (reset/read/wait) | `../tools/pincheck.py` |

## Подключение (C3 → ESP32)

| Канал | Пин C3 | → пин ESP32 | Питает примеры |
|---|---|---|---|
| 1 | GPIO3 | GPIO14 | Active, Light, DeepPullDown, Flash |
| 2 | GPIO4 | GPIO19 | Active, Light |
| 3 | GPIO5 | GPIO26 | DeepPullUp |
| 4 | GPIO6 | GPIO13 | DeepPullUp, MultiSleep, Read/Write/Tone |
| 5 | GPIO7 | GPIO15 | MultiSleep, Read/Write/Tone |
| 6 | GPIO10 | GPIO27 | DeepPullDown |
| — | GND | GND | обязательно; питание между платами НЕ соединять |

3.3-логика у обеих плат — прямое соединение. Пины C3 выбраны свободными от
strap (2/8/9), USB (18/19) и UART-консоли (20/21). На классическом ESP32 как
стимуляторе карта пинов другая — см. `#if CONFIG_IDF_TARGET` в main.cpp.
Перемычку GPIO27→GPIO14 (от стресс-теста #5) нужно **снять** — её функцию
выполняет канал 1, а два push-pull-выхода на одной линии конфликтуют.

## Порты и команда прошивки

- DUT (esp32dev, CH340): **COM11**; стимулятор (USB-Serial-JTAG): **COM12**
  (проверять `pio device list` — порты «плавают»).
- Прошивка стимулятора: `pio run -e test_stimulus -t upload --upload-port COM12`.
  Сборки под C3 запускать с `env -u MSYSTEM` из Git Bash — иначе установщик
  инструментов платформы падает с «MSys/Mingw is not supported».
- Проверка связи: `python tools/stimctl.py COM12 "PING"` → `PONG`.

## Протокол стимулятора

`PING`; `EN/DIS <ch>|ALL` (каналы стартуют hi-Z); `SET <ch> <0|1>`;
`PULSE <ch> <0|1> <ms>` (блокирующий); `WAVE <ch> <half_ms> <count|0=∞>`;
`BOUNCE <ch> <0|1> <n> <gap_ms>` (эмуляция дребезга — охота на окно #3);
`STOP <ch>|ALL`; `FREQ <ch> <ms>` (частотомер: пин канала → вход, подсчёт
rising-фронтов — так проверялся `Pin::tone`); `DUTY <ch> <samples>` (скважность:
N замеров уровня — так проверялись `digitalWrite`/`analogWrite`); `STATE`.

## Грабли, на которые наступили (важно для повторения)

1. **C3 перезагружается при открытии COM-порта**, если pyserial дёргает DTR/RTS
   по умолчанию (USB-Serial-JTAG воспринимает это как паттерн ресета) — состояние
   каналов терялось. Оба скрипта открывают порт с `rts=False, dtr=False` до open.
2. **UART-байты DUT, отправленные при закрытом порте хоста, теряются** —
   стимулировать только при открытом ридере (фоновый stimctl + одновременно
   читающий pincheck).
3. **CH340 замераживает UART FIFO при уходе чипа в сон** — в примеры добавлен
   флаш `beforeEnterSleep(100 мс)` (приём из State/SleepModes).
4. Прошивка DUT (esptool) конфликтует с параллельным читателем порта — не
   запускать pincheck одновременно с upload.

## Отчёт о проверке стенда: 2026-11-08

- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`);
  DUT `esp32dev` COM11, стимулятор `esp32-c3-devkitm-1` COM12
- Тулчейн riscv-gcc 14.2.0 установлен вручную (архив crosstool-NG, разложен по
  образцу xtensa-пакета в `C:\.platformio\packages\toolchain-riscv32-esp`) —
  мета-пакет платформы не ставится из-за MSYS-детекта.
- Все команды протокола отработали на железе (PING/STATE/EN/SET/PULSE/WAVE);
  проводка всех 6 каналов подтверждена пробником PinProbe (уровни pull-направлений
  и прохождение фронтов). Стендом проверены 9 Pin-примеров — см. их README.
