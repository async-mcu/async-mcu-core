# async-mcu-core

Асинхронный планировщик задач для **ESP32** (ESP-IDF поверх Arduino, сборка через PlatformIO). Подробности — в `README.md`.

## Сборка / прошивка / монитор

### Устройство
- Порт: **COM10**, USB-SERIAL CH340.
- Проверить: `pio device list`.

### Модель сборки (`platformio.ini`)
- `src_dir = examples` — каждый пример = отдельный env.
- Env собирает ОДИН пример + всю библиотеку `src/`:
  `build_src_filter = -<*> +<Category/Name> +<../src/*.cpp>`.
- Примеры лежат в `examples/<Category>/<Name>/main.cpp`.
- Для примера без env добавить секцию:
  ```
  [env:<name>]
  build_src_filter = -<*> +<Category/Name> +<../src/*.cpp>
  ```
- Существующие env: `debugging`, `duration`, `boot`,
  `executor_{active,light,deep,mixed,tick,demand,sleep_control,active_cancel}`,
  `state_{simple,save,reactive,sleep_modes}`, `setting_simple`, `pin_*`, `http_simple`.

### Команды
- **Сборка:** `pio run -e <env>` (холодная ~30 с).
- **Прошивка:** `pio run -e <env> -t upload` (~20–30 с, автодетект COM10).
  Косметический `UnicodeEncodeError` (cp1251) в прогресс-выводе на Windows —
  **не влияет**, прошивка успешна (искать `[SUCCESS]`).
- **Монитор:**
  ```
  PYTHONIOENCODING=utf-8 timeout <N> pio device monitor -p COM10
  ```
  - `PYTHONIOENCODING=utf-8` **обязателен** — иначе `UnicodeEncodeError` (cp1251).
  - `timeout <N>` — иначе монитор висит вечно.
  - При подключении сбрасывает плату (DTR/RTS) → свежий `setup()`.
- **Сборка + прошивка + монитор одной строкой:**
  ```
  pio run -e <env> -t upload 2>&1 | tail -4 && \
  PYTHONIOENCODING=utf-8 timeout 8 pio device monitor -p COM10 2>&1 | grep -E "MAIN|EXECUTOR|SLEEP|sleep_start|cancel|tick" | tail -20
  ```
- **Фильтр загрузочного шума:**
  `| grep -vE "^(---|Esp32Exception|ets Jul|configsip|clk_drv|load:|entry|mode:)"`.
- **Стирание flash (вкл. NVS):** `pio run -e <env> -t erase`.
  Обычная прошивка **НЕ стирает NVS** — `Setting`/`Preferences` переживают перепрошивку.

### Декодирование крашей (backtrace)
Встроенный `esp32_exception_decoder` ищет `.pio/build/debugging/firmware.elf`
(не тот env) и не работает. Использовать addr2line вручную:
```
C:/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-addr2line.exe \
  -pfiaC -e .pio/build/<env>/firmware.elf 0x<addr> 0x<addr> ...
```

### Подводные камни (наблюдения)
- **Deep sleep:** `rst:0x5 (DEEPSLEEP_RESET)` при каждом пробуждении → чистый
  вывод на каждую загрузку.
- **Light sleep:** USB-serial (CH340) во время устойчивого light sleep почти не
  отдаёт данные (устройство не перезагружается). Опираться на логи
  `beforeEnterSleep` / `esp_*_sleep_start`, которые печатаются в окне бодрствования.
- **Ложная диагностика IntelliSense** (clangd/MSVC): `identifier "п" is undefined` /
  `expected a ';'` на кириллических комментариях — игнорировать; GCC собирает
  нормально (проверять через `pio run`).
- **Verbose-логи с разыменованием опциональных членов** (напр.
  `Task::getInterval()` у DELAY-задач, где `interval == nullptr`) крашат при
  включённом verbose-логе `TAG_EXECUTOR`. Добавлять с защитой.
