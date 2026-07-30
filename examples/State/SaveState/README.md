# State / SaveState

`State<T>` в RTC-памяти — значение переживает **deep sleep** (полный сброс чипа).

## Что демонстрирует

Счётчик загрузок:

```cpp
RTC_DATA_ATTR State<int> counter(0);
```

Каждая загрузка инкрементирует его и уходит в deep sleep. Благодаря `RTC_DATA_ATTR`
объект `counter` (и его `currValue`) размещён в RTC-памяти и не обнуляется при
`DEEPSLEEP_RESET`.

## Почему конструктор не сбрасывает значение

`State<T>::State(T)` сознательно пропускает инициализацию, если объект лежит в RTC-памяти
**и** пробуждение произошло из deep sleep:

```cpp
State(T value) {
    if (!isRtcDataVariable(this) || esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        currValue = value;
        prevValue = value;
    }
}
```

- **Первый старт** (`POWERON_RESET`) или объект **не** в RTC — значение инициализируется
  аргументом конструктора.
- **Пробуждение из deep sleep** (`DEEPSLEEP_RESET`) и объект в RTC — значение
  **сохраняется** из предыдущего запуска.

## Цикл примера

1. Старт: `counter` = сохранённое значение (или `0` при первом запуске).
2. `counter.set(prev => prev + 1)` — инкремент; `onChange` логирует `(prev, current)`.
3. Через 100 мс — `esp_deep_sleep_start()` на 1 с.
4. Пробуждение → новый `setup()` → шаг 1 (значение пережило сброс).

## Ожидаемый вывод (несколько циклов)

```
I (..) MAIN: Count of boots 1
I (..) MAIN: onChange count of boots: prev 0, current 1
...
I (..) MAIN: Count of boots 2
I (..) MAIN: onChange count of boots: prev 1, current 2
...
```

Каждое пробуждение увеличивает счётчик на 1. `prev`/`current` корректны, т.к.
`RTC_DATA_ATTR` сохранил объект между сбросами.

> **State vs Setting.** `State` в RTC хранит значение только пока есть питание
> (RTC-память живёт лишь во время sleep и обнуляется при обесточивании). Для
> персистентности **вне** питания (NVS/flash) используйте `Setting<T>` —
> см. [`examples/Setting/Simple`](../../Setting/Simple).

## Сборка и прошивка

```bash
pio run -e state_save -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM10
```

При подключении монитор сбрасывает плату (`POWERON_RESET`) → счётчик стартует с `0`.
Последующие пробуждения из deep sleep уже не сбрасывают плату по питанию, поэтому счётчик
растёт.
