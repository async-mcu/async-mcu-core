# Pin / MultiSleepInterrupt

Смешанные режимы: `Deep`-прерывание на pin13 будит из deep sleep, а из его колбэка
динамически регистрируется `Light`-прерывание на pin15 (и отменяется по `FALLING`
на pin13).

## Отчёт о проверке

- **Дата:** 2026-11-08
- **Стенд:** DUT `esp32dev` (COM11) + стимулятор ESP32-C3 (COM12, см. `auxiliary/`);
  каналы 4:GPIO6→13 и 5:GPIO7→15, общий GND
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`)
- **Изменения при проверке:** добавлен флаш `beforeEnterSleep` (100 мс, см.
  LightSleepInterrupt) и раскомментированы VERBOSE-логи PIN/INTERRUPT для диагностики.
- **Результат: OK.** Deep-часть и жизненный цикл pin15 работают:
  SET 4 1 → `DEEPSLEEP_RESET` (ext0 HIGH на 13) → **RISING pin13** → в колбэке
  регистрируется Light-RISING pin15 (`gpio_set_intr_type 15 POSEDGE`,
  `gpio_wakeup_enable pin 15 to 5`) → чип уходит в light (глобальный режим стал
  Light) → SET 4 0 → пробуждение + **FALLING pin13** → `cancel()` pin15 → снова deep.
  Цикл повторяется.

```
rst:0x5 (DEEPSLEEP_RESET)
I (65) MAIN: pin13_RTC onInterrupt::RISING, time: 3003 ms
D (172) PIN: gpio_wakeup_enable pin 15 to 5      <- динамическая регистрация Light
I (176) EXECUTOR: esp_light_sleep_start ...      <- режим стал Light
I (167) MAIN: pin13_RTC onInterrupt::FALLING, time: 7696 ms   <- отпускание будит
I (168) EXECUTOR: esp_deep_sleep_start ...
```

- **Найден дефект библиотеки (issues.md #28, открыт):** динамически
  зарегистрированный Light-фронт **не будит чип из light sleep**. Подача HIGH на
  pin15 (канал 5) во время light-фазы — ноль реакции: ни пробуждения, ни
  `pin15_RTC onInterrupt::RISING` (gpio_wakeup при этом вооружён, VERBOSE это
  подтверждает; при pin13 аналогичный механизм в том же примере работает). pin15
  срабатывает только по ISR в окне бодрствования.

## Сборка и прошивка

```bash
pio run -e pin_multi_sleep_interrupt -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```
