# DeepSleepInterruptPullDown

Пробуждение ESP32 из **deep sleep** по сигналу на GPIO. Вариант **PullDown**.

## Что делает

Две RTC-ножки (**14, 27**) настроены как `INPUT_PULLDOWN` — в покое прижаты к GND (LOW).
На каждую навешаны прерывания deep sleep по обоим фронтам (`RISING` и `FALLING`).
После `startAsync()` контроллер уходит в deep sleep и просыпается при изменении
уровня на любой из ножек, логируя сработавший фронт.

## Вариант PullDown (этот пример)

- Режим ножек: `INPUT_PULLDOWN` (pull к **GND**, в покое LOW).
- Пробуждение: подать на ножку **VCC/3.3V** → фронт `RISING`; снять → `FALLING`.

## Подключение

- Ножка **14** → кнопка/провод на **3.3V** (фронт RISING при нажатии).
- Ножка **27** → аналогично.

Противоположный вариант (ножки прижаты к VCC, пробуждение по GND) — см.
[`examples/Pin/DeepSleepInterruptPullUp`](../DeepSleepInterruptPullUp).

## ⚠️ Важно: ограничения deep sleep

1. **Одинаковый pull для всех пинов.** В deep sleep все пины пробуждения обязаны
   иметь один режим pull — либо все `INPUT_PULLUP`, либо все `INPUT_PULLDOWN`.
   Проверка выполняется **при регистрации прерывания** (`Pin::addInterrupt`); при
   несовпадении программа прерывается:
   ```
   Deep sleep: all wake pins must use the same pull (INPUT_PULLUP or INPUT_PULLDOWN)
   ```

2. **Только RISING/FALLING.** В deep sleep доступны только фронтовые прерывания
   `RISING` и `FALLING`. `ONLOW`/`ONHIGH`/`CHANGE` не поддерживаются — пробуждение
   через ext0/ext1 + revert работает с уровнями. При нарушении:
   ```
   Deep sleep interrupts support only RISING and FALLING edges
   ```

Именно поэтому `PullDown` и `PullUp` — два отдельных примера, каждый со своим единым
режимом pull.

Дополнительно: для deep sleep годятся только **RTC-ножки**
(`0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39`).

## 📌 Допустимые пины для deep sleep (PullUp / PullDown)

Для пробуждения из deep sleep годятся только **RTC-ножки**, причём с **внутренними
pull-резисторами** (ножки «только вход» их лишены). И нужно обходить **strapping-пины**:
их уровень при сбросе задаёт режим загрузки, и «не тот» pull может сделать плату
незагружаемой. Общее правило — все пины пробуждения в deep sleep используют один и тот
же pull (см. выше).

### ESP32 (этот пример собирается под него — `board = esp32dev`)

| | |
|---|---|
| **RTC-ножки (deep wake)** | `0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39` |
| **Только вход — без pull** | `34,35,36,37,38,39` → **непригодны** для этих примеров |
| **Strapping** | `0` (LOW → режим прошивки), `12`/MTDI (HIGH → flash 1.8V → не грузится), `2,5,15` |
| **PullUp (покой HIGH)** | годится `13,14,25,26,27,32,33`; **GPIO 12 — нельзя** (MTDI) |
| **PullDown (покой LOW)** | годится `2,4,12,13,14,15,25,26,27,32,33`; **GPIO 0 — нельзя** (режим прошивки) |

### ESP32-S3

| | |
|---|---|
| **RTC-ножки (deep wake)** | `0…21` (RTC_GPIO0…21) |
| **Занято flash/PSRAM** | `26…32` — не использовать |
| **USB (D−/D+)** | `19,20` — занятие отключает USB-JTAG/UART |
| **Strapping** | `0` (LOW → режим прошивки), `3`, `45`/MTDI (HIGH → flash 1.8V), `46` |
| **PullUp (покой HIGH)** | годятся `0…21` кроме strapping `3`; GPIO 0 (HIGH = норма) — ок; MTDI `45` вне RTC-диапазона |
| **PullDown (покой LOW)** | годятся `1…18,21`; **GPIO 0 — нельзя** (LOW → режим прошивки); `3` — осторожно |

### ESP32-C3

| | |
|---|---|
| **RTC-ножки (deep wake)** | `0…5` (остальные GPIO будят только из light sleep) |
| **Занято flash** | `12…17` |
| **USB-JTAG** | `18,19` |
| **Strapping** | `2` (HIGH → flash 1.8V → не грузится, аналог MTDI), `8`, `9` (LOW → режим прошивки) |
| **PullUp (покой HIGH)** | годятся `0,1,3,4,5`; **GPIO 2 — нельзя** (MTDI) |
| **PullDown (покой LOW)** | годятся `0,1,2,3,4,5`; strapping `8,9` вне RTC-диапазона |

> Сверяйтесь с даташитом конкретного модуля: часть ножек может быть занята под flash,
> PSRAM, RGB-LED или USB и не выведена на пины платы.

## Сборка и прошивка

```bash
pio run -e pin_deep_sleep_interrupt_pulldown -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```

## Отчёт о проверке

- **Дата:** 2026-11-08
- **Стенд:** DUT `esp32dev` (COM11, CH340) + стимулятор ESP32-C3 (COM12, см.
  `auxiliary/`): каналы 1:GPIO3→14 и 6:GPIO10→27, общий GND, перемычка 27→14 снята
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`)
- **Изменения при проверке:** добавлен флаш `beforeEnterSleep` (100 мс) — без него
  CH340 теряет хвост UART FIFO при уходе в сон (приём из State/SleepModes).
- **Результат:** OK. Полный цикл эмуляции фронтов на двух пинах:
  deep (ext1 `ANY_HIGH`) → SET 1+6 → `DEEPSLEEP_RESET` + **RISING** на обоих пинах →
  оба ревернуты → deep заблокирован, **light**-фаза (ожидание отпускания) →
  SET 0+0 → **FALLING** на обоих → снова deep → цикл повторяется. 2 полных цикла
  воспроизведены, пропусков нет.

```
rst:0x1 (POWERON_RESET)  →  esp_deep_sleep_start (ext1 ANY_HIGH)
SET 1 1 + SET 6 1:
rst:0x5 (DEEPSLEEP_RESET)
I (247) MAIN: pin14_RTC onInterrupt::RISING, time: 3091 ms
I (259) MAIN: pin27_RTC onInterrupt::RISING, time: 3103 ms
I (265) EXECUTOR: esp_light_sleep_start ...   <- реверты: ждём отпускания в light
SET 1 0 + SET 6 0:
I (371) MAIN: pin14_RTC onInterrupt::FALLING, time: 10212 ms
I (378) MAIN: pin27_RTC onInterrupt::FALLING, time: 10219 ms
I (384) EXECUTOR: esp_deep_sleep_start ...    <- реверты сброшены
```

- **Наблюдение (не блокирует):** на POWERON-старте изредка проскакивает спурт-фронт
  FALLING (~60 мс, без движения линии) — похоже на защёлкнутый статус при
  `gpio_set_intr_type` на уже-LOW пине; в пример не влияет.
- Таймер deep-сна армируется мусорным значением (open #10/#25: `sleep time:
  18446744073…`) — для пробуждения по пинам не мешает.
