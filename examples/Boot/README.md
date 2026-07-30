# Boot

Запуск **без** `setup()`/`loop()`: вся логика — в глобальных объектах `Boot`, а точки входа
Arduino предоставляет сама библиотека.

## Что демонстрирует

Этот пример **не содержит** `setup()` и `loop()`. Их даёт `src/Boot.cpp` (weak-определения):
дефолтный `setup()` зовёт `initAsync()` + `startAsync()`, а `loop()` удаляет задачу Arduino.
Пример, определивший свои `setup()`/`loop()`, перекрывает их (как все остальные примеры).

Вместо этого используются два глобальных объекта `Boot` (по одному на ядро):

- `core0.cpp` → `boot0`: `onRepeat<Deep>` каждые 2000 мс на `CORE0`.
- `core1.cpp` → `boot1`: `onRepeat<Deep>` каждые 2500 мс на `CORE1`.

## Зачем нужен `Boot`

Глобальные конструкторы выполняются **до** `initAsync()`, когда планировщик ещё не готов.
Регистрировать задачи (`onRepeat`/`onDelay`/...) прямо в глобальном объекте нельзя. `Boot`
оборачивает колбэк в `onInit` — он выполнится в фазе INIT, когда `initAsync()` уже отработал:

```cpp
async::Boot boot0([]() {
    onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task &) {
        ESP_LOGI(TAG_CORE0, "Repeat task 0, every 2000 ms, time: %llu ms", rts_ms());
    });
});
```

## Структура

| Файл | Что |
|------|-----|
| `core0.cpp` | `boot0` — задачи `CORE0` (Deep repeat 2000 мс). |
| `core1.cpp` | `boot1` — задачи `CORE1` (Deep repeat 2500 мс). |
| `src/Boot.cpp` | weak `setup()`/`loop()` — дефолтная точка входа. |
| `include/async/Boot.h` | класс `Boot`. |

Разбиение на `core0.cpp`/`core1.cpp` — организационное: ядро назначения задаётся **явным**
аргументом `CORE0`/`CORE1` внутри колбэка (`onRepeat(..., CORE0, ...)`), а не тем, в каком
файле лежит объект.

## Deep sleep, непрерывное время и точный каденс

Обе задачи — `<Deep>`: между срабатываниями чип уходит в deep sleep (полный сброс) и
просыпается по таймеру. Поле `time:` — это `rts_ms()`: `gettimeofday` минус база старта
(захватывается в `initAsync()`, переживает deep sleep в `RTC_DATA_ATTR`). Отсчёт
**начинается с ~0 при запуске** и **растёт непрерывно** между пробуждениями.

**Точный каденс.** Deep sleep = полный ребут каждый цикл, поэтому после wake чип ~95–98 мс
грузится, прежде чем задача сможет выполниться (латентси загрузки). Без компенсации огни
съезжали бы с сетки на эту величину. Поэтому `startAsync()` делает один короткий
**калибровочный deep sleep** (2 мс) до первого дедлайна — на его wake измеряется латентси
(`deep wake latency: … — grid shifted`) и сетка deep-дедлайнов сдвигается вниз на неё. В итоге
**все огни, включая первый**, попадают ровно в дедлайн (2000/4000/…, ±единицы мс). Цена —
`setup()` удлиняется на ~98 мс (калибровочный сон + его wake-boot).

## Ожидаемый вывод

```
I (115) EXECUTOR: deep wake latency: 95838 us (95 ms) — grid shifted
I (115) CORE0: Repeat task 0, every 2000 ms, time: 2000 ms
I (115) CORE1: Repeat task 1, every 2500 ms, time: 2500 ms
I (115) CORE0: Repeat task 0, every 2000 ms, time: 4000 ms
I (115) CORE1: Repeat task 1, every 2500 ms, time: 5000 ms
I (115) CORE0: Repeat task 0, every 2000 ms, time: 6000 ms
I (115) CORE1: Repeat task 1, every 2500 ms, time: 7500 ms
I (115) CORE0: Repeat task 0, every 2000 ms, time: 8000 ms
```

`CORE0` стреляет каждые 2 с, `CORE1` — каждые 2.5 с; **все огни ровно в дедлайн с первого**
(латентси deep-wake скомпенсирована калибровочным сном). Латентси (~95–98 мс) стабильна
между wake, поэтому сдвиг сетки точный и держится ±единицы мс.

## Сборка и прошивка

```bash
pio run -e boot -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM10
```
