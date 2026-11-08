# Pin / ActiveInterrupt

Активные (Active) прерывания GPIO: несколько типов фронтов на одном пине,
динамическая регистрация/отмена прерывания на втором пине из колбэка.

## Что демонстрирует

- `pin14_RTC` (INPUT_PULLUP): `FALLING` + `RISING` + `ONLOW` — на один пин несколько
  подписок; `ONLOW` считает тики, пока линия прижата;
- из колбэка `FALLING` динамически регистрируется `RISING` на `pin19_RTC`
  (INPUT_PULLDOWN), из колбэка `RISING` — отменяется (`cancel()`).

## Отчёт о проверке

- **Дата:** 2026-11-08
- **Стенд:** DUT `esp32dev` (COM11) + стимулятор ESP32-C3 (COM12, см. `auxiliary/`);
  каналы 1:GPIO3→14 (меандр 250 мс) и 2:GPIO4→19 (меандр 300 мс), общий GND
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`)
- **Исправлено при проверке (баги примера):** двойной `startAsync()` (повторный вызов
  делает `esp_system_abort("Already started")` — бут-луп) → первый вызов заменён на
  `initAsync()`, как в остальных примерах.
- **Найден и исправлен баг библиотеки:** `Pin::removeInterrupt` при опустевшем списке
  прерываний своего пина сбрасывал **глобальный** `interruptSleepMode` в None — после
  `cancel()` на pin19 шедулер уходил в deep sleep прямо посреди Active-прерываний
  (с мусорным таймером, open #10). Фикс: пересчёт режима по оставшимся прерываниям
  всех пинов (`src/Pin.cpp`).
- **Контракт (фикс #12, 2026-11-09):** `Interrupt::cancel()` **освобождает объект** —
  указатель после cancel невалиден; в примере указатель обнуляется сразу после
  cancel (защита от двойного удаления при повторном RISING без пере-регистрации).
  Проверено повторной прошивкой: динамическая цепочка register→cancel под
  меандром 250/300 мс работает, 41 диспетч за 8 с, без крашей.
- **Результат после фиксов:** OK. Меандр 250 мс на pin14 — чистая серия
  FALLING/RISING без пропусков (30+ диспетчей за 8 с); меандр на pin19 —
  `pin19_RTC onInterrupt::RISING` с `onLowCount` 37→137→237 и сбросом после cancel,
  ровно по логике примера.
- **Дополнительно 2026-11-08 (фикс #3):** на этом примере верифицирован ре-арм
  эмуляцией дребезга (C3, команда BOUNCE): три грязные пачки (40×2 мс, 60×1 мс,
  20×5 мс), после каждой — чистый контрольный импульс 300 мс. Все три импульса
  дали пары FALLING→RISING (t=4189→4489, 8555→8855, 12938→13238 мс) — пин больше
  не глушится окном ре-арма (фикс: `interrupted=false` до `gpio_intr_enable`).

```
I (260) MAIN: pin14_RTC onInterrupt::FALLING, time: 114 ms
I (510) MAIN: pin14_RTC onInterrupt::RISING,  time: 363 ms
I (760) MAIN: pin14_RTC onInterrupt::FALLING, time: 613 ms
...
I (24805) MAIN: pin19_RTC onInterrupt::RISING, onLowCount 37,  time: 24658 ms
I (25405) MAIN: pin19_RTC onInterrupt::RISING, onLowCount 137, time: 25258 ms
I (27805) MAIN: pin19_RTC onInterrupt::RISING, onLowCount 37,  time: 27658 ms  <- сброс после cancel
```

## Сборка и прошивка

```bash
pio run -e pin_active_interrupt -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```
