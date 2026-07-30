# State / SleepModes

`State::set` в трёх sleep-режимах: **deep**, **light**, **active**.

## Что демонстрирует

`onChange` фиксирован на Active: `State::set()` планирует колбэк через `onOnce`, а у
`onOnce` режим закреплён `SleepMode::Active` (`Executor.cpp:237`). Сменить режим самого
`onChange` нельзя — поэтому «режим set» здесь задаётся вручную через `setSleepMode()`.

`onRepeat<Deep>` каждые 3 с циклически переключает режим `Deep → Light → Active → Deep`
и инкрементит `value`:

```cpp
RTC_DATA_ATTR State<int> value(0);   // RTC: переживает DEEPSLEEP_RESET

onRepeat<Deep>(Duration::ms(3000), [](Task &) {
  switch (getSleepMode()) {            // Deep/None→Light, Light→Active, Active→Deep
    ...
  }
  setSleepMode(next);
  value.set([](int v) { return v + 1; });
});
```

Колбэки `beforeEnterSleep` / `afterWakeUp` показывают **реальный** сон каждого режима:

| Режим `setSleepMode` | Что происходит | В логах |
|----------------------|----------------|---------|
| **Deep** | `esp_deep_sleep` → `DEEPSLEEP_RESET` при каждом пробуждении; `value` в RTC переживает сброс и растёт. | `beforeEnterSleep: Deep`, после сброса — `afterWakeUp: Deep`. |
| **Light** | `esp_light_sleep`, чип не перезагружается. | `beforeEnterSleep: Light`, `afterWakeUp: Light`. |
| **Active** | Сна нет — колбэк выполняется тут же. | `beforeEnterSleep` **не** зовётся (сна нет). |

## Почему режимы по одному, а не тремя одновременными onRepeat

Постоянная `onRepeat<Active>` инкрементит `activeTasksCount` (`Executor.cpp:176`) и
удерживает чип активным (`Executor.cpp:399`). Три одновременных счётчика оставили бы чип
в Always-Active — и реального deep/light сна не случилось бы. Поэтому режимы здесь
переключаются по одному через `setSleepMode`.

## Примечание о логах light sleep

USB-serial (CH340) во время устойчивого light sleep почти не отдаёт данные (см. `CLAUDE.md`
→ «Подводные камни»). Строки light-фазы могут появляться с задержкой или скопом после
пробуждения — это особенность адаптера, а не планировщика. Deep-фаза даёт чистый вывод на
каждую загрузку (`DEEPSLEEP_RESET`).

Чтобы логи успели покинуть UART до ухода в сон, колбэк `beforeEnterSleep` заканчивается
`vTaskDelay(pdMS_TO_TICKS(100))` — без этой задержки CH340 регулярно теряет последние
строки прямо перед light/deep sleep. Тот же приём — в
[`Executor/SleepControl`](../../Executor/SleepControl) и [`State/SaveState`](../SaveState).

## Ожидаемый вывод (схематично)

```
I (..) MAIN: SleepModes running: Deep -> Light -> Active cycle, 3 s/step
I (..) MAIN: beforeEnterSleep: Deep          <- стартовый Deep → первый сон (3 с) → сброс
... (DEEPSLEEP_RESET) ...
I (..) MAIN: afterWakeUp: Deep
I (..) MAIN: setSleepMode -> Light
I (..) MAIN: onChange: 0 -> 1
I (..) MAIN: beforeEnterSleep: Light         <- light sleep (3 с)
I (..) MAIN: afterWakeUp: Light
I (..) MAIN: setSleepMode -> Active
I (..) MAIN: onChange: 1 -> 2
                                             <- Active: beforeEnterSleep НЕ зовётся (сна нет)
I (..) MAIN: setSleepMode -> Deep
I (..) MAIN: onChange: 2 -> 3
I (..) MAIN: beforeEnterSleep: Deep          <- снова deep sleep → сброс
...
```

`value` накапливается через все сбросы (RTC). В `Active`-фазе `beforeEnterSleep`
отсутствует — это и есть «без сна».

## Сборка и прошивка

```bash
pio run -e state_sleep_modes -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM10
```
