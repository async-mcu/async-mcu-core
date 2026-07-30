# State / Simple

Наблюдаемое значение `State<T>` и четыре способа его установить.

## Что демонстрирует

`State<T>` хранит значение и оповещает подписчиков через `onChange(prev, current)`.
Установить значение можно четырьмя способами:

| Способ | Сигнатура | Поведение |
|--------|-----------|-----------|
| **`set(value)`** | `void set(T)` | Безусловно меняет значение и **всегда** триггерит `onChange`. |
| **`set(callback)`** | `T set(std::function<T(T)>)` | Новое значение = `callback(текущее)`; всегда триггерит. Возвращает новое значение. |
| **`setIfChanged(value)`** | `bool setIfChanged(T)` | Меняет значение и триггерит `onChange` **только если** оно отличается. Возвращает `true`, если изменилось. |
| **`setIfChanged(callback)`** | `T setIfChanged(std::function<T(T)>)` | То же, но новое значение = `callback(текущее)`. Возвращает текущее значение. |

`setIfChanged` удобен, когда колбэк дорогой или даёт побочные эффекты (например, запись в
NVS у наследника `Setting`): повторная установка того же значения не делает лишней работы.

### Несколько подписчиков `onChange`

`onChange` можно вызывать несколько раз — колбэки складываются в `std::vector` и
срабатывают **все** одним отложенным таском. У `value` в примере их два: `onChange`
(логирует переход) и `observer` (реагирует на новое значение) — на каждое изменение
печатается пара строк.

### Nullable: `State<T*>`

С указательным типом `State` работает как опциональное значение: `setNull()` — это
`set(nullptr)`, `isNotNull()` проверяет наличие значения. В примере `State<int*> handle`
берёт/отдаёт адрес переменной и логирует переходы через `nullptr` в `onChange`.

## Важное: `onChange` — отложенный

Колбэки `onChange` не вызываются синхронно внутри `set`/`setIfChanged`. Они планировируются
через `onOnce` и выполняются в следующем цикле планировщика. Поэтому в мониторе синхронные
логи из `setup()` появляются **раньше** строк `onChange`.

## Ожидаемый вывод

```
I (..) MAIN: setIfChanged(11): changed=0      <- 11 == 11, нет изменения
I (..) MAIN: setIfChanged(42): changed=1      <- 11 -> 42
I (..) MAIN: setIfChanged(cb same): 42        <- 42 -> 42, нет изменения
I (..) MAIN: setIfChanged(cb +100): 142       <- 42 -> 142
I (..) MAIN: final value: 142
I (..) MAIN: isNotNull (init): 0              <- handle = nullptr
I (..) MAIN: isNotNull (set):  1              <- handle = &resource
I (..) MAIN: isNotNull (null): 0              <- handle = nullptr (setNull)
I (..) MAIN: onChange: prev 0 -> current 10   <- отложенные onChange для value ...
I (..) MAIN: observer: value is now 10        <- ... второй подписчик на то же set
I (..) MAIN: onChange: prev 10 -> current 11
I (..) MAIN: observer: value is now 11
I (..) MAIN: onChange: prev 11 -> current 42
I (..) MAIN: observer: value is now 42
I (..) MAIN: onChange: prev 42 -> current 142
I (..) MAIN: observer: value is now 142
I (..) MAIN: handle onChange: prev 0x0 -> current 0x3ff...  <- nullptr -> &resource
I (..) MAIN: handle onChange: prev 0x3ff... -> current 0x0  <- setNull()
```

У `value` **два подписчика**, поэтому на каждое из 4 изменений печатается пара строк
(`onChange` + `observer`). Два вызова `setIfChanged` с тем же значением (`11` и
`cb same`) **не** породили `onChange`. Для `handle` (`State<int*>`) `onChange`
сработал дважды: `nullptr -> &resource` и обратно через `setNull()`.

## Сборка и прошивка

```bash
pio run -e state_simple -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM10
```
