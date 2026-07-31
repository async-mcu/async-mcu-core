# Executor / Mixed

Смешанные sleep-режимы в одном скетче: `Deep`, `Light` и `Active` задачи, включая
вложенное планирование (`onDelay` из колбэка другой задачи) и разбивку по ядрам.

## Что демонстрирует

Три блока задач с разными режимами сна:

| Задача | Режим | Интервал | Ядро | Вложенность |
|--------|-------|----------|------|-------------|
| `onDelay` 1 | Deep | 2 с | CORE0 | → Light 1.3 с (CORE1) → Active 500 мс (CORE1) |
| `onRepeat` 2 | Deep | 5 с | CORE1 | → Light 700 мс (CORE1) → Active 300 мс (CORE1) |
| `onRepeat` 2 | Light | 3 с | CORE1 | — |

Вложенные `onDelay` планируются **из колбэка** родительской задачи: сработал
`onDelay<Deep>` → внутри себя ставит `onDelay<Light>`, тот — `onDelay<Active>`. Так
строится цепочка «проснулся из deep → подождал в light → короткая активность (например,
поднять WiFi)».

## Какой сон фактичен

Несмотря на наличие `onRepeat<Deep>`, на устройстве преимущественно используется
**light sleep** (видно по `esp_light_sleep_start` в логах `EXECUTOR`). Причина — nested
`onDelay<Active>`: пока активна хоть одна Active/Tick-задача, планировщик не уходит в deep
(`activeTasksCount`, `Executor.cpp:399`). Deep sleep случается в окнах между ними
(стартовый `onDelay<Deep>` даёт `DEEPSLEEP_RESET`, после которого видна
`deep wake latency … grid shifted`).

## Логи

При сборке с `-DUSE_ESP_IDF_LOG` (а `CONFIG_LOG_DEFAULT_LEVEL` закомментирован) строки
`ESP_LOGI(TAG_MAIN,…)` по умолчанию не печатаются. Поэтому в начале `setup()` уровень
задаётся явно:

```cpp
esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO);   // видеть esp_*_sleep_start
```

Перед `initAsync()` также стоит `vTaskDelay(pdMS_TO_TICKS(100))`, чтобы стартовый лог
успел покинуть UART до первого ухода в сон.

## Ожидаемый вывод

```
I (29)  MAIN: Before script time 35 ms
I (130) EXECUTOR: >>> Async started (reset_reason=1)
I (130) EXECUTOR: start deepTasksTimeFast[0][0] to 2000000   <- onDelay<Deep> 2 с
I (130) EXECUTOR: start deepTasksTimeFast[1][0] to 5000000   <- onRepeat<Deep> 5 с
I (130) EXECUTOR: start deepTasksTimeFast[1][1] to 3000000   <- onRepeat<Light> 3 с
I (315) EXECUTOR: deep wake latency: 195990 us — grid shifted
I (332) MAIN: onDelay 1 Deep 2000ms,  time: 1868, core: 0
I (337) MAIN: onDelay 1 Light 1300ms, time: 1873, core: 1
I (842) MAIN: onDelay 1 Active 500ms, time: 2378, core: 0
I (843) MAIN: onRepeat 2 Light 3000ms, time: 3064, core: 1
I (854) MAIN: onRepeat 2 Deep 5000ms,  time: 4868, core: 1
I (860) MAIN: onDelay 2 Light 700ms,  time: 4875, core: 1
I (1164) MAIN: onDelay 2 Active 300ms, time: 5178, core: 0
I (1165) MAIN: onRepeat 2 Light 3000ms, time: 6064, core: 1
```

`onDelay 1 Deep` срабатывает на ~2 с и внутри себя планирует цепочку `Light → Active`;
`onRepeat 2 Light` тикает каждые 3 с; `onRepeat 2 Deep` — каждые 5 с со своей вложенной
цепочкой.

## Сборка и прошивка

```bash
pio run -e executor_mixed -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM10
```
