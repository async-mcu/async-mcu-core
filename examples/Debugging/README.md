# Debugging

Пример проверки логирования фреймворка. Полезен при отладке собственных примеров:
показывает, как включать и фильтровать логи по TAG и уровню.

## Что делает

1. **Системные TAG** — выводит по одной строке каждым из 7 системных TAG из
   `include/async/Logging.h` (`EXECUTOR`, `PIN`, `TASK`, `INTERRUPT`, `CHAIN`,
   `HTTP_SERVER`, `SETTING`): видно, каким префиксом подписывается каждый модуль.
2. **Уровни логгирования** — печатает пачку строк E/W/I/D/V тега `DEMO` при
   понижении порога `esp_log_level_set(TAG_DEMO, …)`: VERBOSE → INFO → WARN →
   ERROR → NONE. По исчезновению строк виден текущий порог. Тот же
   `esp_log_level_set` применим к любому системному TAG (в примере — хелпер
   `setAllSystemTagsLevel`).
3. **Периодическая диагностика планировщика** (каждые 10 с, режим Active): время,
   свободная куча, свободный стек worker'ов и список задач по ядрам. Active-задачи
   не попадают в `coreTasks` (живут на `esp_timer`) — поэтому в списке 0 задач,
   это ожидаемо.

Технические заметки:
- `<async/Logging.h>` включается в `main.cpp` ПЕРВЫМ: его `LOG_LOCAL_LEVEL` /
  `ESP_LOG_MAXIMUM_LEVEL` должны вступить в силу до первого включения `esp_log.h`,
  иначе `ESP_LOGD`/`ESP_LOGV` вырежутся при компиляции.
- Диагностика — Active-задача: `esp_timer` держит чип бодрствующим, и логи
  гарантированно уходят в UART (при устойчивом light sleep CH340 почти не отдаёт
  данные, см. CLAUDE.md).

## Отчёт о проверке

- **Дата:** 2026-11-07
- **Платформа:** `pioarduino/platform-espressif32#55.03.35`, плата `esp32dev`,
  порт **COM11** (USB-SERIAL CH340; в CLAUDE.md указан COM10 — порт переехал)
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1**
- **Env:** `debugging` — `pio run -e debugging -t upload` (сборка SUCCESS ~31 с,
  прошивка OK, порт захвачен `--upload-port COM11` из-за автодетекта)
- **Результат:** OK. Все 7 системных TAG вывелись своими префиксами; пороги
  отфильтровали строки в точности по ожиданию — 5/3/2/1/0 строк при
  VERBOSE/INFO/WARN/ERROR/NONE, runtime-VERBOSE подтверждён (строка `V (69)`);
  периодическая диагностика приходит каждые ~10 с.

Фрагмент лога (COM11, 115200):

```
I (8) MAIN: === Debugging example ===
I (9) MAIN: Arduino-ESP32 3.3.5, ESP-IDF 5.5.1, free heap 333912
I (9) MAIN: 1) System TAGs from <async/Logging.h>: 7
I (12) EXECUTOR: system TAG 1/7: TAG_EXECUTOR -> "EXECUTOR"
I (17) PIN: system TAG 2/7: TAG_PIN -> "PIN"
I (21) TASK: system TAG 3/7: TAG_TASK -> "TASK"
I (25) INTERRUPT: system TAG 4/7: TAG_INTERRUPT -> "INTERRUPT"
I (31) CHAIN: system TAG 5/7: TAG_CHAIN -> "CHAIN"
I (35) HTTP_SERVER: system TAG 6/7: TAG_HTTP_SERVER -> "HTTP_SERVER"
I (41) SETTING: system TAG 7/7: TAG_SETTING -> "SETTING"
I (46) MAIN: 2) esp_log_level_set(TAG_DEMO, ...):
I (51) MAIN: threshold VERBOSE -> expect all 5 lines
E (55) DEMO: level demo: ERROR   (1)
W (59) DEMO: level demo: WARN    (2)
I (62) DEMO: level demo: INFO    (3)
D (65) DEMO: level demo: DEBUG   (4)
V (69) DEMO: level demo: VERBOSE (5)
I (72) MAIN: threshold INFO -> expect E/W/I only
E (76) DEMO: level demo: ERROR   (1)
W (79) DEMO: level demo: WARN    (2)
I (83) DEMO: level demo: INFO    (3)
I (86) MAIN: threshold WARN -> expect E/W only
E (90) DEMO: level demo: ERROR   (1)
W (94) DEMO: level demo: WARN    (2)
I (97) MAIN: threshold ERROR -> expect E only
E (101) DEMO: level demo: ERROR   (1)
I (104) MAIN: threshold NONE -> expect no lines
I (109) MAIN: restore TAG_DEMO and system TAGs to INFO
I (114) EXECUTOR: >>> Async started (reset_reason=1)
I (10339) MAIN: Time 10120 ms, mem 320732
I (10339) MAIN: Core: 0, free stack space: 9144, tasks: 0
I (10340) MAIN: Core: 1, free stack space: 9292, tasks: 0
```
