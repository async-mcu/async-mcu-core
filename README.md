# async-mcu-core

Асинхронный планировщик задач для **ESP32** (ESP-IDF поверх Arduino-фреймворка).

Библиотека предоставляет единый API для планирования задач (`onDelay`, `onRepeat`,
`onOnce`, `onTick`, `onDemand`, `onInit`) с прозрачной интеграцией режимов
энергопотребления. Каждая задача связывается с одним из трёх режимов сна, и
планищик сам выбирает, какой сон использовать, исходя из набора активных задач и
состояния прерываний.

## Ключевые особенности

- **Мультиядерность.** Две задачи FreeRTOS (`mainLoop`) работают на `CORE0`/`CORE1` и
  опрашивают собственные списки задач. Каждая задача может быть привязана к ядру.
- **Три режима сна:**

  | Режим      | Механика                                              | Пробуждение          |
  |------------|-------------------------------------------------------|----------------------|
  | `Active`   | Аппаратный `esp_timer` (one-shot / periodic)          | — (CPU не спит)      |
  | `Light`    | `esp_light_sleep_start`, задача живёт в памяти        | GPIO / timer         |
  | `Deep`     | `esp_deep_sleep_start`, тайминг переживает сон в RTC  | ext0 / ext1 GPIO     |

- **Переживание deep sleep.** Время и флаги глубокого сна хранятся в
  `RTC_DATA_ATTR`, поэтому повторяющиеся deep-задачи корректно продолжают
  отсчёт после пробуждения.
- **Пробуждение по GPIO с поддержкой revert-логики** (ext0/ext1), включая
  согласование режимов pull для нескольких пинов.
- **Колбэки `beforeEnterSleep` / `afterWakeUp`** для подготовки периферии ко сну.
- **Ручной режим сна** (`setSleepMode`) — ограничение максимально
  допустимого уровня сна.

## Компоненты

| Модуль        | Назначение |
|---------------|------------|
| **Executor**  | Ядро планировщика: цикл `mainLoop` на каждом ядре, расчёт времени до ближайшей задачи, выбор и вход в light/deep сон, координация ядер через мьютекс. |
| **Task**      | Единичная задача: тип (`REPEAT`, `DELAY`, `DEMAND`, `TICK`, `ONCE`, `INIT`, `INTERR`), режим сна, тайминг, привязка к ядру, колбэк. |
| **Duration**  | Абстракция интервала времени с микросекундным разрешением (внутренне `uint64_t`), фабрики `Duration::us/ms`, арифметика и операторы сравнения. |
| **Pin**       | Обёртка над GPIO: цифровые/аналоговые чтение и запись, PWM и `tone()` через LEDC, ADC (`adc_oneshot`), конфигурация pull-режимов. |
| **Interrupt** | GPIO-прерывания с учётом сна (Active/Light/Deep), приоритизация режима пина, регистрация в глобальном списке источников пробуждения. |
| **State**     | Наблюдаемое значение с колбэками `onChange`. RTC-aware: сохраняет значение при выходе из deep sleep, если размещена в RTC-памяти. |
| **Setting**   | `State` + персистентность в NVS (`Preferences`). Специализации для `int`, `float`, `double`, `bool`, `String` с авто-сохранением при изменении. |
| **Boot**      | Запуск пользовательского кода на старте через `onInit`; предоставляет `setup()`/`loop()` (инициализирует и стартует планировщик). |
| **HttpServer**| REST + WebSocket сервер поверх `esp_http_server` с JSON (cJSON), сессиями, фильтрами (напр. Basic-Auth), multipart и чанковой отдачей. |
| **Stream**    | Базовый интерфейс асинхронного потока (чтение/запись/seek). Реализации: `ByteStream`, `FileStream`. |
| **Semaphore** | Простой счётный семафор для координации доступа к ресурсам. |
| **Chain**     | Построение последовательностей операций (delay → then → interrupt → loop). *В разработке — код временно отключён.* |

## Быстрый старт

```cpp
#include <Async.h>
using namespace async;

void setup() {
  initAsync();

  // однократно через 2 с (deep sleep)
  onDelay<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    // ...
  });

  // повтор каждые 5 с (light sleep)
  onRepeat<Light>(Duration::ms(5000), CORE1, [](Task &) {
    // ...
  });

  // колбэки вокруг сна
  beforeEnterSleep([](SleepMode m) { /* подготовка периферии */ });
  afterWakeUp([](SleepMode m)      { /* восстановление */ });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
```

Примеры для каждого режима и компонента находятся в каталоге [`examples/`](examples).

**Примечание:** каталоги примеров с префиксом `-` (например `examples/Pin/-ReadPin`, `examples/Pin/-Tone`, `examples/Pin/-WritePin`, `examples/-Chain`) помечены как **устаревшие или не готовые**. Они не обновляются и могут не собираться — ориентируйтесь на примеры без этого префикса. 

## Требования

- Платформа: ESP32 (Espressif32, IDF v5.x).
- Фреймворк: Arduino.
- Сборка: PlatformIO.

## Память: строковые литералы и логирование

Все строковые литералы (форматы `ESP_LOG*`, теги, ключи NVS, JSON-строки и т.п.)
попадают в секцию `.flash.rodata` — то есть во **flash**, а не в RAM: на ESP32
`.rodata` мапится во flash через кэш. Объём небольшой, но при дефиците памяти им
можно управлять.

**Статический след строк** в `include/` + `src/` (после объединения одинаковых
литералов компоновщиком GCC `-fmerge-constants`):

| Показатель | Значение |
|---|---|
| Всего литералов (с дублями) | 113 |
| Уникальных литералов | 102 |
| Вклад в `.rodata` | ≈ 2.5 KiB |
| Лидеры | `Executor.cpp` (0.85 KiB), `Pin.cpp` (0.71), `HttpServer.cpp` (0.40), `Task.cpp` (0.33) |

Подавляющая часть — форматные строки `ESP_LOGV`/`ESP_LOGD` (диагностика
deep-sleep: «Deep sleep: all wake pins must use the same pull…» и подобные).
Их объём напрямую зависит от уровня логирования.

### Управление через `LOG_LOCAL_LEVEL`

Порог задаётся в [`include/async/Logging.h`](include/async/Logging.h) (по
умолчанию `ESP_LOG_VERBOSE`). Это **compile-time** отсечение: каждый `ESP_LOGx`
раскрывается в `if (LOG_LOCAL_LEVEL >= level) esp_log_write(...)`, и если уровень
вызова выше порога — компилятор удаляет и сам вызов, и его форматную строку.
Поэтому снижение уровня уменьшает **и** `.text`, **и** `.rodata`.

Замер на env `executor_mixed` (вся библиотека `src/` + один пример), чистая
сборка. Размеры — всей секции прошивки (фреймворк + наш код), Δ — наш вклад:

| `LOG_LOCAL_LEVEL` | `.flash.rodata` | `.flash.text` | Δ `.rodata` | Δ `.text` |
|---|---:|---:|---:|---:|
| `VERBOSE` (по умолчанию) | 84 964 | 144 200 | — | — |
| `DEBUG`               | 84 180 | 143 196 | −784   | −1 004 |
| `INFO`                | 83 844 | 142 904 | −1 120 | −1 296 |
| `WARN`                | 82 916 | 142 036 | −2 048 | −2 164 |
| `NONE`                | 82 788 | 141 948 | −2 176 | −2 252 |

Переход `VERBOSE → INFO` экономит ≈ 1.1 KiB строк и ≈ 1.3 KiB кода;
`VERBOSE → NONE` — ≈ 2.2 KiB строк и ≈ 2.2 KiB кода (суммарно ≈ 4.3 KiB flash).

> **Повышение** уровня (выше `VERBOSE` невозможно — это максимум) ничего не
> добавляет: макросы `ESP_LOGV` уже включены в сборку. Влияние на рост памяти есть
> только при добавлении **новых** вызовов логирования в код.

> Полную дельту даёт только `NONE`: он убирает **все** логи, включая `ESP_LOGE`.
> Уровни `INFO`/`WARN` сохраняют сообщения об ошибках/предупреждениях.

> `ESP_LOG_MAXIMUM_LEVEL` и `CONFIG_LOG_DYNAMIC_LEVEL_CONTROL` (там же в
> `Logging.h`) управляют **runtime**-фильтрацией — порогом по тегам через
> `esp_log_level_set()`. На размер строк они не влияют (только на массив уровней
> в RAM), поэтому для экономии flash менять нужно именно `LOG_LOCAL_LEVEL`.

### Как замерять

PlatformIO в конце сборки печатает только суммарный объём Flash
(`Flash: xx.x% (used NNNNN bytes)…`), но строки лежат в `.flash.rodata`, а код — в
`.flash.text`. Для раздельного учёта нужен `xtensa-esp32-elf-size` из toolchain:

```bash
# размер секций одной сборки (замените <env>, напр. на executor_mixed)
C:/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size.exe \
  -A .pio/build/<env>/firmware.elf | grep -E 'flash.rodata|flash.text'
```

> Замеры **корректны только после чистой сборки** (`pio run -e <env> -t clean`):
> инкремент может не выбросить «мёртвые» строки после смены уровня лога.

**Сравнение уровней вручную:** поменять `#define LOG_LOCAL_LEVEL` в
[`include/async/Logging.h`](include/async/Logging.h) на нужный
(`ESP_LOG_DEBUG` / `INFO` / `WARN` / `NONE`) → чистая сборка → снять
`.flash.rodata` командой выше → повторить для каждого уровня → вернуть
`ESP_LOG_VERBOSE`.

**Автоматический прогон** всех уровней (Windows / Git Bash) — печатает таблицу
`level / .rodata / .text` для env `executor_mixed` и восстанавливает файл:

```bash
cd /e/PlatformIO/async-mcu-core
PIO=~/.platformio/penv/Scripts/platformio.exe
SIZE='C:/.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-size.exe'
ELF=.pio/build/executor_mixed/firmware.elf
set_lvl(){ python - "$1" <<'PY'
import re,sys
p=r'include/async/Logging.h';b=open(p,'rb').read()
b=re.sub(rb'(?m)^#define LOG_LOCAL_LEVEL[^\r\n]*',('#define LOG_LOCAL_LEVEL '+sys.argv[1]).encode(),b)
open(p,'wb').write(b)
PY
}
for L in VERBOSE DEBUG INFO WARN NONE; do
  set_lvl ESP_LOG_$L
  "$PIO" run -e executor_mixed -t clean >/dev/null 2>&1
  "$PIO" run -e executor_mixed >/dev/null 2>&1
  out=$("$SIZE" -A "$ELF" 2>/dev/null)
  r=$(printf '%s\n' "$out"|grep '\.flash\.rodata'|awk '{print $2}')
  t=$(printf '%s\n' "$out"|grep '\.flash\.text'|awk '{print $2}')
  printf '%s\t%s\t%s\n' "$L" "$r" "$t"
done
set_lvl ESP_LOG_VERBOSE   # вернуть умолчание
```