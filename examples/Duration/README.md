# Duration

Арифметика и сравнение `Duration`: сложение/вычитание, `after`/`before`, замер
реального интервала в `loop()`.

## Что демонстрирует

- `Duration::ms(...)` — фабрика владеющих `Duration*`;
- `add`/`subtract` — арифметика (возвращает `Duration` **по значению**);
- `after`/`before` — сравнение длительностей;
- `Duration::now()` + `subtract` — замер интервала (~1 c в `loop()`).

## ВАЖНО: `add`/`subtract` возвращают объект, а не число

```cpp
auto sum = duration1->add(*duration2);  // sum — это Duration (объект)
```

Передавать его напрямую в вариадик `ESP_LOGx("...", sum)` **нельзя** — это UB
(нет неявной конверсии в `uint64_t` внутри `...`, в `%d` попадает мусор).
Извлекайте значение и печатайте `%llu`:

```cpp
ESP_LOGI(TAG_MAIN, "Sum: %llu ms", sum.ms());
```

## Отчёт о проверке

- **Дата:** 2026-11-07
- **Платформа:** `pioarduino/platform-espressif32#55.03.35`, плата `esp32dev`,
  порт COM11 (USB-SERIAL CH340)
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1**
- **Исправлено при проверке (баги примера, не библиотеки):**
  1. Пример не вызывал `esp_log_level_set` → при дефолтном уровне лога ниже INFO
     прошивка работала, но **полностью молчала**. Добавлен
     `esp_log_level_set(TAG_MAIN, ESP_LOG_INFO)` (как в остальных примерах).
  2. Объекты `Duration` передавались в вариадик `ESP_LOGx` (UB): во все строки
     попадала одна и та же мусорная константа `1073422920`. Значения теперь
     извлекаются через `.ms()`, формат `%llu`.
- **Результат после фиксов:** OK — `Sum: 4500 ms`, `Difference: 1500 ms`; ветки
  `after`/`before` выбираются корректно; `loop()` тикает 999–1000 ms.

```
I (8) MAIN: Sum: 4500 ms
I (9) MAIN: Difference: 1500 ms
I (9) MAIN: 3 seconds have not yet passed.
I (9) MAIN: 1.5 seconds have not yet passed.
I (1011) MAIN: Difference random: 1000 ms
I (2011) MAIN: Difference random: 999 ms
...
```

## Сборка и прошивка

```bash
pio run -e duration -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```
