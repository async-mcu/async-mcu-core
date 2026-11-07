# State / Reactive

«Живая» реактивность: значение `State<T>` мутирует **по таймеру**, а `onChange`
подписчики реагируют на каждое изменение в цикле планировщика.

## Что демонстрирует

В примерах [`Simple`](../Simple) и [`SaveState`](../SaveState) все вызовы `set()`
делаются однократно в `setup()`. Здесь — наоборот: значение меняется само,
`onRepeat` каждые 2 с инвертирует `State<bool>`, и поток изменений обрабатывается
уже во время работы планировщика.

```cpp
State<bool> flag(false);

flag.onChange([](bool prev, bool current) { ... });  // подписчик 1: логгер
flag.onChange([](bool prev, bool current) { ... });  // подписчик 2: исполнитель

onRepeat<Active>(Duration::ms(2000), [](Task &) {
  flag.set([](bool v) { return !v; });               // инверсия каждые 2 с
});
```

Плюс — **несколько подписчиков** на один `State`: колбэки хранятся в `std::vector`
и срабатывают **все** одним отложенным таском. В примере их два:

| Подписчик | Роль | Поведение |
|-----------|------|-----------|
| **логгер** | наблюдатель | Печатает переход `prev -> current`. |
| **исполнитель** | сторона эффекта | Реагирует на новое значение. В реальном проекте здесь было бы включение реле/LED, запрос в сеть и т.п.; пример без железа — только лог. |

## Важное: `onChange` — отложенный

Колбэки `onChange` не вызываются синхронно внутри `set`. Они планировируются через
`onOnce` и выполняются отдельным тиком планировщика. Поэтому строки `logger` /
`executor` появляются уже **после** того, как `onRepeat` отработал и передал
управление дальше.

## Ожидаемый вывод

```
I (..) MAIN: Reactive running: flag toggles every 2 s (logs only, no hardware)
I (2000) MAIN: logger:   prev 0 -> current 1
I (2000) MAIN: executor: action for state=1
I (4000) MAIN: logger:   prev 1 -> current 0
I (4000) MAIN: executor: action for state=0
I (6000) MAIN: logger:   prev 0 -> current 1
I (6000) MAIN: executor: action for state=1
...
```

Каждые 2 с — **пара** строк (оба подписчика): `set()` один раз меняет значение,
после чего срабатывают и логгер, и исполнитель.

## Сборка и прошивка

```bash
pio run -e state_reactive -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```

## Отчёт о проверке

- **Дата:** 2026-11-07
- **Платформа:** `pioarduino/platform-espressif32#55.03.35`, плата `esp32dev`,
  порт COM11 (USB-SERIAL CH340)
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1**
- **Результат:** OK — инверсия строго по сетке 2000 мс (лог на 2126, 4126, 6126,
  8126 мс — джиттер в пределах 1 мс), на каждое изменение — пара строк (оба
  подписчика: `logger` + `executor`). Active-задача удерживает чип бодрствующим,
  логи идут без потерь.

```
I (9) MAIN: Reactive running: flag toggles every 2 s (logs only, no hardware)
I (2126) MAIN: logger:   prev 0 -> current 1
I (2126) MAIN: executor: action for state=1
I (4126) MAIN: logger:   prev 1 -> current 0
I (4126) MAIN: executor: action for state=0
...
```
