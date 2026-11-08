# Pin / LightSleepInterrupt

Прерывания в режиме **Light**: чип спит лёгким сном, будится по уровню GPIO
(`gpio_wakeup`), а фронты RISING/FALLING эмулируются реверсом wake-уровня после
каждого пробуждения. Динамическая регистрация/отмена `RISING` на pin19 из колбэков.

## Отчёт о проверке

- **Дата:** 2026-11-08
- **Стенд:** DUT `esp32dev` (COM11) + стимулятор ESP32-C3 (COM12, см. `auxiliary/`);
  канал 1:GPIO3→14 (импульсы к GND 400 мс), общий GND
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`)
- **Изменения при проверке:** добавлен флаш `beforeEnterSleep` (100 мс) — CH340
  теряет хвост UART FIFO при уходе в light sleep, без него даже строка
  `esp_light_sleep_start` не покидала чип (приём из State/SleepModes).
- **Результат:** OK. Полный цикл: light sleep (wake LOW на pin14) → импульс к GND →
  пробуждение + **FALLING** + регистрация `RISING` на pin19 (`gpio_wakeup_enable
  pin 19 to 5`) → реверс pin14 на HIGH → отпускание линии будит само → **RISING** +
  `cancel()` pin19 → сон. Реверты wake-уровня (4↔5) видны в VERBOSE-логе; шкала
  `rts` непрерывна через сны. 2+ цикла, повторяемо.

```
I (754) EXECUTOR: esp_light_sleep_start from core 1 ...
V (754) PIN: onDemand pin 14
V (754) PIN: gpio_wakeup_enable revert pin 14 to 5   <- ждём отпускания
I (757) MAIN: pin14_RTC onInterrupt::FALLING, time: 48900 ms
D (763) INTERRUPT: Create interrupt pin 19 ... sleepMode Light
V (855) PIN: gpio_wakeup_enable pin 14 to 4
I (855) MAIN: pin14_RTC onInterrupt::RISING, time: 49297 ms
D (855) INTERRUPT: Cancel interrupt pin 19
```

- Таймер light-сна армируется мусорным значением (open #10/#25) — пробуждению по
  GPIO не мешает.
- Особенность стенды: логи, отправленные чипом при **закрытом** COM-порте хоста,
  теряются — читать порт нужно непрерывно во время стимула (см. `tools/pincheck.py`).

## Сборка и прошивка

```bash
pio run -e pin_light_sleep_interrupt -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```
