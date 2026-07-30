# Issues — аудит async-mcu-core

> **Этот документ самодостаточен** — рассчитан на чтение в новой сессии без истории
> разработки. Все проблемы проверены чтением исходного кода (не статическими
> подозрениями). Дата аудита: 2026-07-30. Покрытие: `include/async/*.h`, `src/*.cpp`
> (~3580 LOC). Статус: **#1, #5, #14, #17, #21 исправлены**, **#4 закрыт** (не баг), **#6 частично** (слот регистрации), остальные — открыты.

Тяжесть: 🔴 критичная (краш/память/гонка) · 🟠 высокая (DoS/некорректное поведение) ·
🟡 средняя (пограничные случаи/утечки) · ⚪ низкая (API/стиль/компоновка).

---

## 0. О проекте и сборке

**async-mcu-core** — асинхронный планировщик задач для **ESP32** (ESP-IDF поверх Arduino,
FreeRTOS, **двухъядерный**). Сборка через PlatformIO. Подробности — в `README.md` и
`CLAUDE.md`; здесь только необходимое для работы над багами.

- **Целевая плата:** классический ESP32 (`board = esp32dev`), порт **COM10**.
- **Сборка env:** каждый пример = свой env в `platformio.ini`. Релевантные для багов:
  `pin_deep_sleep_interrupt_pullup`, `pin_deep_sleep_interrupt_pulldown`,
  `pin_active_interrupt`, `pin_light_sleep_interrupt`, `http_simple`, `executor_*`.
- **Сборка:** `pio run -e <env>` (холодная ~30 с).
- **Прошивка+монитор:** см. `CLAUDE.md` (`PYTHONIOENCODING=utf-8` обязательно).
- **Текущее состояние репо (ветка `upd`):** недавно добавлен макрос
  `ASYNC_EXT1_WAKEUP_LOW` (`include/async/Executor.h`) — `ALL_LOW` на классическом ESP32,
  `ANY_LOW` на S3/S2/C3; убран мёртвый `#include <esp_gatt_defs.h>` из `Uuid.h` (теперь
  проект собирается и под S3). Пример `DeepSleepInterruptPullUp` переведён с GPIO12 на
  GPIO26 (MTDI-проблема).

---

## 1. Архитектура (нужно понимать контекст багов)

### Планировщик
- `startAsync()` создаёт по задаче FreeRTOS `mainLoop` на **каждое ядро** (pinned), каждая
  итерирует свой список `coreTasks[core]` (`src/Executor.cpp:17`).
- **Цикл `mainLoop`** (`src/Executor.cpp:237-480`): сканирует `coreTasks[core]`, собирает
  «дозревшие» задачи в `toExecute`, выполняет, считает per-core флаги
  (`lightTasksExists`, `tickTasksExists`, `minSleepTime`), публикует их, затем решает, спать
  ли. Координация сна — через `sleepReadyMutex` (`:36`): **только одно ядро** заходит в
  критическую секцию входа в sleep и выбирает Light/Deep.
- **Deep sleep = полный сброс чипа** → `setup()` выполняется заново при каждом
  пробуждении. Переменные `RTC_DATA_ATTR` переживают сброс (`deepTasksTime`,
  `deepInterruptsRevert`, `deepInterruptsMode`).

### Типы задач (`Task::Type`) и флаги
- `INIT` (выполняется один раз в `initAsync`), `ONCE` (один раз и `delete`), `TICK`
  (каждый цикл), `DELAY` (сработать в момент `next`), `REPEAT` (каждый интервал).
- Флаг **`certainly`** = «выполни в следующем цикле» (используется `onDemand`/`onOnce`/
  `onInit`).

### Режимы сна (`SleepMode`) и где живёт тайминг задачи
- **Active** — задача крутится на `esp_timer` (не зависит от планировщика); удерживает чип
  бодрствующим (`activeTasksCount`). Тайминг — в `esp_timer`.
- **Light** — тайминг в `Task::next`; ядро может light-sleep между срабатываниями.
- **Deep** — тайминг хранится в **позиционном** массиве `deepTasksTimeFast[core][i]`
  (рабочая копия) и `RTC_DATA_ATTR deepTasksTime` (переживает сброс); копируется перед
  уходом в deep sleep (`Executor.cpp:426-430`).

### Диспетч прерываний
- GPIO ISR `Pin::ISR` (`src/Pin.cpp:347`) → `gpio_intr_disable` → `Pin::interrupt()`
  (`:41`) → если `!interrupted`, выставить флаг и положить `Pin*` в статическую очередь
  `irqQueue` (через `xQueueSendFromISR`).
- Задача `Pin::irqDispatchTask` (`Pin.cpp:32`, закреплена на **ядро 0**, `:28`) читает
  очередь и зовёт `pin->interruptTask->schedule()` → `onOnce(core, this)` → кладёт
  onDemand-задачу пина (`certainly=true`) в `coreTasks[core]`.
- `mainLoop` выполняет onDemand-задачу пина: переконфигурирует wake-уровень (toggle
  `revert`), **включает** `gpio_intr` обратно, диспетчерит user-callback'и по типу фронта,
  сбрасывает `interrupted`.

### Revert (эмуляция фронтов в deep/light sleep)
ext0/ext1 и gpio_wakeup будят по **уровню**. Чтобы emulate фронты `RISING`/`FALLING`,
после каждого пробуждения флаг `revert` тогглируется и wake-уровень инвертируется —
следующий sleep будет ждать противоположного уровня. `RTC_DATA_ATTR bool
deepInterruptsRevert` переживает deep sleep.

### ext0/ext1 выбор
1 wake-пин → `esp_sleep_enable_ext0_wakeup`; >1 → `esp_sleep_enable_ext1_wakeup_io`
(`Executor.cpp:444-460`). Уровень: PULLDOWN→`ANY_HIGH`, PULLUP→`ASYNC_EXT1_WAKEUP_LOW`
(`ALL_LOW` на ESP32 = «все кнопки», `ANY_LOW` на S3+ = «одна кнопка»).

### ⚠️ Ключевой инвариант, порождающий гонки
**`setup()` Arduino выполняется на ядре 1** → объекты `Pin` по умолчанию на `CORE1` → их
`interruptTask` на ядре 1. Но `irqDispatchTask` закреплён на **ядре 0** → он пишет в
`coreTasks[1]` с ядра 0, пока `mainLoop` ядра 1 итерирует этот же вектор. Это корень бага
#2.

---

## 2. Ключевые структуры и глобальные переменные

| Сущность | Объявление | Назначение / замечание |
|---|---|---|
| `coreTasks[core]` | `Executor.cpp:17` | per-core `vector<Task*>`; итерируется `mainLoop` **без блокировки** |
| `deepTasksTimeFast[core][i]` | `Executor.cpp:23` | тайминги Deep-задач **по позиции**; `DEEP_TASKS_STACK=2` (`Definitions.h`) |
| `deepTasksTime[core][i]` | `Executor.cpp:22` (`RTC_DATA_ATTR`) | переживает deep sleep |
| `minSleepTime[core]` | `Executor.cpp:30` | `uint64_t`; кросс-ядерное чтение без атомики |
| `activeTasksCount` | `Executor.cpp:32` | `std::atomic<int>` — сам декремент атомарен, **но** check-then-decrement — нет |
| `sleepReadyMutex` | `Executor.cpp:36` | охраняет **только** секцию входа в sleep, не `coreTasks` |
| `globalInterruptParams` | `Interrupt.cpp:12` | `vector<Interrupt*>`; мутация `:60-72` **без блокировки** |
| `deepInterruptsRevert/mode` | `Interrupt.cpp:10-11` (`RTC_DATA_ATTR`) | состояние deep-wake между сбросами |
| `Pin::interrupted`, `Pin::revert` | `Pin.h:33-34` | флаги диспетча; окна гонки #3 |
| `Pin::interruptParams` | `Pin.h:36` | `vector<Interrupt*>`; у `Pin` **нет деструктора** |

---

## 3. Глоссарий

- **Active / Light / Deep** — режимы задачи (см. §1).
- **onDemand / onOnce** — задача с `certainly=true`, выполняется в следующем цикле.
- **revert** — тоггл wake-уровня для эмуляции фронтов.
- **ext0 / ext1** — источники пробуждения RTC: ext0 — один пин по уровню, ext1 —
  несколько (классический ESP32: `ALL_LOW`/`ANY_HIGH`; S3+: `ANY_LOW`/`ANY_HIGH`).
- **RTC-пины** — пины, способные будить из deep sleep (классический ESP32:
  `0,2,4,12,13,14,15,25,26,27,32-39`; 34-39 — только вход, **без** внутренних pull).
- **DEEP_TASKS_STACK** — лимит (`=2`) числа Deep-задач, хранимых в `deepTasksTimeFast`.

---

## 4. Проблемы

### ✅ 1. ~~Double-free в `onRepeat`~~ — ИСПРАВЛЕНО (2026-07-30)
**Где:** `include/async/Executor.h:118` и `:134` → `src/Task.cpp:21-30`
**Код:**
```cpp
// Executor.h:117
Task * onRepeat(Duration * interval, Core core, ...) {
    return onRepeat<mode>(interval, interval, core, callback);   // тот же указатель дважды
}
// Task.cpp:21  ~Task()
if(delay != nullptr)  { delete delay;  delay = nullptr;  }
if(interval != nullptr){ delete interval; interval = nullptr; }  // double-free
```
**Контекст:** 3-арг перегрузки передают один `Duration*` и как `delay`, и как `interval`;
`~Task` удаляет оба без проверки алиасинга.
**Воспроизведение:** создать `onRepeat<Light>(Duration::ms(500), cb)` и `cancel()` его (или
любое уничтожение repeat-задачи) → повреждение кучи/краш.
**Фикс (применён):** `~Task` сделан устойчивым к алиасингу — добавлен флаг
`delayIsInterval = (delay != nullptr && interval == delay)`, и `interval` удаляется только
если он не совпадает с `delay` (`src/Task.cpp`). Сборка `executor_light` — OK. Отдельный
start-delay в `onRepeat` не понадобился: алиасинг предусмотрен дизайном («interval он же
start delay»).
**Связано:** #15.

### 🔴 2. Гонка на `coreTasks[core]` — краш на любом активном прерывании
**Где:** запись `src/Executor.cpp:213-216` (`onOnce` push) из `Pin::irqDispatchTask`
(`Pin.cpp:32-39`, ядро 0) vs итерация `Executor.cpp:255-328` (erase `:260,:268,:324`)
**Контекст:** см. инвариант §1 — `setup()` на ядре 1 → пин на `CORE1`, а диспетчер на ядре
0 пишет в `coreTasks[1]`. `std::vector` без блокировки → реаллокация под итератором.
**Воспроизведение:** любой пример с `addInterrupt<Active>` + дёрганье линии
(`pin_active_interrupt`) → спорадический Guru Meditation при обработке прерывания.
**Фикс:** единый мьютекс на `coreTasks[core]` (взять в `onOnce`/`onTick`/`onDelay`/`onInit`
и в цикле `mainLoop`), либо lock-free очередь отложенных задач с единым потребителем.
**Связано:** #13 (тот же класс — общая стратегия блокировок).

### 🔴 3. Потерянный фронт необратимо «глушит» пин
**Где:** `src/Pin.cpp:104` (`gpio_intr_enable`) vs `:142` (`interrupted=false`); ISR
`:347-352`; `interrupt()` `:41-63`
**Контекст:** в onDemand сначала **включается** прерывание (`:104`), затем user-callback'и,
и только в конце сбрасывается `interrupted` (`:142`). Фронт в окне `[104,142)` → ISR
делает `gpio_intr_disable` (`:349`), видит `interrupted==true`, дропает без очереди. После
`:142` прерывание выключено, а ре-арм (`:104`) живёт только внутри onDemand, который больше
не запустится.
**Воспроизведение:** `addInterrupt<Active>(FALLING, …)` на кнопку с дребезгом → после
первого «глухого» окна пин перестаёт выдавать прерывания до ребута.
**Фикс:** сначала `interrupted=false`, затем `gpio_intr_enable`; либо ре-арм по
`digitalRead()` после `:142`.
**Связано:** #5.

### ✅ 4. ~~Ложное срабатывание всех INPUT_PULLUP пинов при пробуждении~~ — НЕ БАГ (перепроверено)
**Где:** `src/Pin.cpp:295-298`
**Код:**
```cpp
bool shouldInterrupt = (cause == ESP_SLEEP_WAKEUP_EXT0)
    || (currentMode == INPUT_PULLUP)                                  // выглядит подозрительно, но…
    || (digitalRead() == !getDeepInterruptsRevert() ? … : …)
    || (esp_sleep_get_ext1_wakeup_status() & (1ULL << pinNum));
```
**Почему это НЕ баг.** Операнд `(currentMode == INPUT_PULLUP)` действительно заставляет
`this->interrupt()` сработать для каждого pullup-пина при любом ext0/ext1-пробуждении. НО
multi-pin pullup на классическом ESP32 будится через ext1 **`ALL_LOW`** (макрос
`ASYNC_EXT1_WAKEUP_LOW`, `Executor.cpp:455`) — чип просыпается **только когда все wake-пины
LOW**. Следовательно будить «одной кнопкой» в принципе нельзя: чтобы проснуться, **все**
pullup-пины обязаны быть прижаты к GND, т.е. на каждом реальный фронт FALLING. Так что
срабатывание всех пинов корректно, а не ложно. Сценарий «нажал пин 26 → idle-пин 13 ложно
логирует RISING» **невозможен** при `ALL_LOW`. Операнд в худшем случае избыточен
(A=`ext0`, D=`ext1_wakeup_status` уже покрывают реальный пин), но вреда не несёт.
**«All buttons required» — это и есть `ALL_LOW`**, аппаратное ограничение классического
ESP32 (задокументировано в `examples/Pin/DeepSleepInterruptPullUp/README.md`), а не код-баг.
**Возможный остаточный мелкий эффект (низкая уверенность, не подтверждён):** в revert-цикле
(после FALLING будит → ждём `ANY_HIGH` на отжатие) удерживаемый LOW-пин может повторно дать
FALLING без нового фронта. Спекулятивно.
**Статус:** закрыто — не баг.

### ✅ 5. ~~ISR не в IRAM + `digitalRead()` из прерывания~~ — ИСПРАВЛЕНО (2026-07-30)
**Где:** `src/Pin.cpp:347-352` → `interrupt()` `:41-42`
**Контекст:** `Pin::ISR` и `Pin::interrupt()` (в ней `digitalRead()`) не помечены
`IRAM_ATTR`. Совпадение фронта с отключённым flash-кешем (запись NVS/Preferences, OTA) →
`Cache disabled but cached memory accessed`. `ESP_DRAM_LOGV` корректен; проблема в
`digitalRead()`/теле `interrupt()`.
**Воспроизведение:** активное прерывание + любая flash-операция (собственный `Setting`
её делает); автономный стресс-тест — `examples/Pin/ActiveInterruptFlash` (молотит NVS,
пока летят фронты GPIO27→GPIO14).
**Фикс (применён):** `IRAM_ATTR` проставлен на всю ISR-цепочку — `ISR`, `interrupt()`,
`digitalRead()`, `getPin()`, `Task::setValue()`; добавлен `#include "hal/gpio_ll.h"`.
Уровень пина и ре-арм выключения читаются через inline-HAL `gpio_ll_get_level` /
`gpio_ll_intr_disable` (register-only, инлайнятся в IRAM). Лог — `ESP_DRAM_LOGV`.
Сборка `pin_active_interrupt_flash` — OK.
**⚠️ Замечание к исходной формулировке фикса:** предложение «уровень через
`gpio_get_level` (он в IRAM)» **неверно** для этого IDF (Arduino-ESP32 на ESP-IDF 5.x) —
высокоуровневые `gpio_get_level`/`gpio_intr_disable` лежат **во flash**. Применён
правильный вариант — `gpio_ll_*` (inline-HAL).
**🟡 Остаточный латентный эффект (сейчас не проявляется):** в `ESP_DRAM_LOGV` формат лежит
в DRAM (`DRAM_STR`, `esp_log.h:475`), но тег `TAG_PIN` (`"PIN"`, `Logging.h`) — указатель
на **flash**, читается через `%s`. При дефолтном уровне INFO ветка VERBOSE мертва
(`_ESP_LOG_EARLY_ENABLED` → false) → тег не читается → безопасно. Если поднять глобальный
уровень до VERBOSE (`CONFIG_LOG_MAXIMUM_LEVEL=5` + default=VERBOSE) — лог в ISR снова
полезет во flash в окне cache-disable. Робастно: `ESP_DRAM_LOGV(DRAM_STR("PIN"), …)` либо
убрать этот verbose-лог из ISR.
**Связано:** #3.

### 🔴 6. Рассинхрон `deepTasksTimeFast` после удаления задачи
**Где:** чтение по индексу `src/Executor.cpp:277`; erase `:260, :268, :324, :488`;
запись в `onDelay` `:131-133`
**Контекст:** тайминги Deep-задач хранятся **по позиции** `deepTasksTimeFast[core][i]`.
Удаление более ранней задачи (INIT в `initAsync:488`, ONCE/TICK/Light в цикле) сдвигает
индексы вниз, массив не пересинхронизируется → Deep-задача читает чужой слот.
**Воспроизведение:** `onInit(...)` или ONCE-задача, объявленная **перед** Deep-задачей →
Deep-задача стреляет не в своё время (одноразовая может сработать повторно).
**Фикс:** хранить `next` в самом `Task` (как для Light), убрать позиционную привязку; либо
пересчитывать слоты при каждом erase.
**Статус (2026-07-30):** частично починено для случая **регистрации** — слот
`deepTasksTimeFast` теперь считается как число уже зарегистрированных DEEP-задач (а не
`coreTasks.size()`), так что INIT-задача в начале списка больше не завышает индекс и не
заставляет deep-задачу срабатывать сразу при старте. Runtime-случай (стирание ONCE/TICK/Light
в цикле сдвигает индекс уже бегущей deep-задачи) — всё ещё открыт.

### 🟠 7. `activeTasksCount` — двойной декремент (TOCTOU)
**Где:** `src/Executor.cpp:80-96` (`callTaskExecute`, `notifyActiveTaskCancelled`);
`src/Task.cpp:106-117` (`cancel`)
**Контекст:** оба пути `if (isCounted()) { activeTasksCount--; setCounted(false); }` —
неатомарный компаунд (сам `--` атомарен, `activeTasksCount` — `std::atomic`, но проверка
`counted` + декремент + очистка — нет). Гонка таймера и `cancel()` → декремент на 2 →
счётчик в минус → `mainLoop:349` неверно решает о сне.
**Воспроизведение:** `onDelay<Active>` с короткой задержкой + `cancel()` из другой задачи
в момент срабатывания.
**Фикс:** CAS-цикл, либо отдельный мьютекс active-lifecycle, либо единая точка
декремента под блокировкой.
**Связано:** #9, #15.

### 🟠 8. Неограниченные аллокации в HTTP — удалённый DoS
**Где:** `src/HttpServer.cpp:119` (`body.reserve(_req->content_len)`) и `:219`
(`std::vector<uint8_t> buf(frame.len …)`)
**Контекст:** размер из контролируемых клиентом `Content-Length` / длины WS-кадра без
верхнего предела. На `-fno-exceptions` (ESP32) → OOM/abort/ребут.
**Воспроизведение:** env `http_simple` — `POST` с `Content-Length: 0xFFFFFFFF` или
WS-кадр с огромной длиной → немедленный ребут, без аутентификации.
**Фикс:** `cap = min(len, MAX_BODY)` (~4–8 КБ); при превышении — отброс тела / 413.

### 🟠 9. Утечка одноразовых Active-задач (и `Pin::tone`)
**Где:** `src/Executor.cpp:80-89`; `src/Pin.cpp:390-400` (`tone`)
**Контекст:** сработавший `onDelay<Active>` декрементит счётчик, но не удаляет `Task` и не
зовёт `esp_timer_delete`. Active-задач нет в `coreTasks`, `mainLoop` их не убирает.
`tone()` создаёт такой каждый вызов.
**Воспроизведение:** повторные `pin.tone(freq, duration)` → истощение кучи и пула
`esp_timer`.
**Фикс:** в `callTaskExecute` после одноразового Active — `esp_timer_delete` + `delete task`.
**Связано:** #7, #15.

### 🟡 10. Underflow времени сна → таймер будит через ~2⁶⁴ мкс
**Где:** `src/Executor.cpp:408` (light) и `:472` (deep); охрана только `!= UINT64_MAX`
(`:407`, `:471`)
**Контекст:** между снятием дедлайна (начало цикла) и
`esp_sleep_enable_timer_wakeup(deadline - now)` выполняются `beforeEnterSleepCallback` и
конфиг GPIO; если «now» уже прошло → беззнаковое вычитание переполняется.
**Воспроизведение:** короткий интервал Light/Deep-задачи + долгий `beforeEnterSleep` →
таймер не срабатывает, ждём только GPIO.
**Фикс:** пересчитать «now» перед `enable_timer_wakeup`; если дедлайн прошёл — не спать
или кап в 0.

### 🟡 11. UB `1 << pin` для RTC-пинов ≥ 32
**Где:** `src/Executor.cpp:365` и `:436`
**Контекст:** дедуп `pinsMask & (1 << pin)` — `1` это `int`, `pin` бывает 32–39. Сдвиг ≥32
— UB. Маска копится правильно (`1ULL<<` в `:367,:438`), но `revertedPinsCount`/дедуп криво.
**Фикс:** `1ULL <<` в обоих местах.

### 🟡 12. Утечка объектов `Interrupt`
**Где:** `src/Pin.cpp:310-344` (`removeInterrupt`); `src/Interrupt.cpp:20-29` (`~Interrupt`,
`cancel`); у `Pin` **нет деструктора** (`Pin.h`)
**Контекст:** `cancel()`/`removeInterrupt()` убирают указатель из векторов, но не
`delete`. `Interrupt::~Interrupt()` зовёт `removeInterrupt`, так что явный `delete`
работает, но API не подталкивает к нему; у `Pin` нет `~Pin()` для `interruptParams`.
**Воспроизведение:** runtime `interrupt->cancel()` или уничтожение `Pin` с прерываниями →
утечка `std::function` и объекта.
**Фикс:** `delete` в `removeInterrupt`/`cancel` (с учётом, что `~Interrupt` тоже зовёт
remove — защитить от двойного удаления); добавить `~Pin()`.

### 🟡 13. Гонка на `globalInterruptParams` (и sleep/wake-колбэки)
**Где:** итерация `src/Executor.cpp:364, :435`; мутация `src/Interrupt.cpp:60-72`
**Контекст:** ядро со взятым `sleepReadyMutex` итерирует глобальный список, а второе ядро
(ещё не спит) в это время может `add/removeGlobalInterruptParam` (без мьютекса) →
реаллокация под итератором. То же касается
`beforeEnterSleepCallback`/`afterWakeUpCallback`.
**Воспроизведение:** `interrupt->cancel()` из колбэка/задачи во время, когда другое ядро
считает маску пробуждения.
**Фикс:** тот же мьютекс/стратегия, что и для #2.
**Связано:** #2.

### ✅ 14. ~~NVS-ключ длиннее 15 символов молча не пишется~~ — ИСПРАВЛЕНО (2026-07-30)
**Где:** `include/async/Setting.h` — хелпер `assertNvsKeyLength()` + вызов в каждом
Setting-конструкторе (int/float/double/bool/String); `TAG_SETTING` в `Logging.h`.
**Контекст:** лимит `NVS_KEY_NAME_MAX_SIZE-1` (15 символов; сам макрос = 16 с учётом
нул-терминатора) раньше был лишь в комментарии. `putInt`/`putString` молча вернёт 0 →
настройка не персистится.
**Фикс (применён):** fail-fast — если `strlen(key) > NVS_KEY_NAME_MAX_SIZE-1`, логируется
`ESP_LOGE(TAG_SETTING, …)` и зовётся `esp_system_abort("Setting: NVS key longer than 15 chars")`.
Паттерн тот же, что у `Pin::addInterrupt` (инварианты RTC-pin / edge-type). Сборка
`setting_simple` — OK.
**Пример (краш при загрузке):**
```cpp
Setting<int> badKey(0, "brightness_level");  // 16 символов > 15 → abort на старте
```
```
E (xxx) SETTING: NVS key 'brightness_level' too long: 16 chars, max 15 (NVS_KEY_NAME_MAX_SIZE-1)
abort: Setting: NVS key longer than 15 chars
Backtrace: 0x...        // затем reboot → boot-loop, пока ключ не исправят в прошивке
```

### 🟡 15. UAF-окно в `~Task`
**Где:** `src/Task.cpp:21-35`
**Контекст:** сначала `delete delay/interval`, потом `esp_timer_delete`. Если колбэк таймера
в полёте и трогает `getDelay()/getInterval()` → чтение освобождённой памяти. Плюс удаление
Active-задачи из её же колбэка → self-`esp_timer_delete` → дедлок.
**Фикс:** сначала `esp_timer_delete` (точка синхронизации), потом освобождать члены.
**Связано:** #1, #7, #9.

### ⚪ 16. Несогласованное владение в фабриках `Duration`
**Где:** `src/Duration.cpp:56-66`
`zero()/us()/ms()` возвращают владеющий `Duration*`, а `now()/maximum()` — по значению →
естественное `auto d = Duration::ms(1500)` утекает (так в `examples/Duration/main.cpp`).
Удаляются только `onDelay`/`onRepeat`, забирающие `Duration*`.

### ✅ 17. ~~`State::set` зовёт `onChange` без проверки изменения~~ — ИСПРАВЛЕНО (2026-07-30)
**Где:** `include/async/State.h` — новый метод `setIfChanged(T)`; `include/async/Setting.h` —
гард `if (prev == current) return;` в каждой `onChange`-ламбде.
**Контекст:** `set(T)` безусловно обновляет значение и планировирует колбэк. Для `Setting`
каждое избыточное `set(same)` → `prefs.begin/put/end` → лишняя запись NVS (износ flash).
**Фикс (применён):**
- **`State<T>::setIfChanged(T)`** — новый метод: при `value == currValue` не меняет `prevValue`
  и **не** планировирует колбэки (возвращает `bool` — изменилось ли). Существующий `set(T)`
  оставлен безусловным — его могут звать ради принудительного re-trigger. Есть и перегрузка
  **`setIfChanged(std::function<T(T)>)`** — новое значение вычисляется колбэком из текущего
  (как у `set(callback)`), возвращает текущее значение. Демо всех 4 вариантов —
  `examples/State/Simple`.
- **`Setting`** — обязательная защита на самой границе NVS: `if (prev == current) return;`
  перед `prefs.begin(...)` в `onChange`. Срабатывает **независимо** от того, через `set` или
  `setIfChanged` пришли (Setting наследует `setIfChanged` без override).
Сборка `setting_simple` — OK.

### ⚪ 18. `RouteData`/`WsData` (`user_ctx`) не освобождаются
**Где:** `src/HttpServer.cpp:231` и др.
`new RouteData{…}`/`new WsData{…}` сохраняются в `httpd_uri_t::user_ctx` и нигде не
удаляются. Разовый липк на маршрут; накапливается при пересоздании `HttpServer`.

### ⚪ 19. Неконстантное сравнение токена в `BasicAuthFilter`
**Где:** `src/HttpServer.cpp:249`
`req.header("Authorization") != _auth_header` — обычное `std::string::operator!=`,
шорт-схемится на первом байте → timing-оракул на Base64-кред в теории.
**Фикс:** сравнение за константное время.

### ⚪ 20. Объявлённые, но не определённые методы
**Где:** `include/async/HttpServer.h:45` (`WsResponse::sendBinary`), `:58`, `:59`
(`HttpRequest::cookie`, `HttpRequest::query`)
`HttpResponse::sendBinary`/`cookie` определены, аналоги для `Ws*`/`HttpRequest` — нет.
Любой вызов → `undefined reference` (link error).
**Фикс:** реализовать либо удалить объявления.

### ✅ 21. Deep sleep: латентси пробуждения сдвигала огни с сетки дедлайнов — ИСПРАВЛЕНО (2026-07-30)
**Где:** `src/Executor.cpp` — калибровочный сон в `startAsync`, измерение+сдвиг в начале цикла
`mainLoop`, `RTC_DATA_ATTR deepWakeLatencyUs` / `calibIntendedWake`.
**Контекст:** Deep sleep = полный ребут чипа каждый цикл. RTC-таймер будит вовремя, но после
wake чип ~95–98 мс грузится (bootloader → setup → планировщик), прежде чем задача может
выполниться. Поэтому Deep-задачи физически срабатывают на ~латентси позже дедлайна — огни
съезжают с сетки 2000/4000 на ~+96 мс. (Попутно выяснили: deep-`now` через `rtcBoot+esp_timer`
опережает `rts_us` на ~46 мс → вариант `-46`; правильная шкала — `rts_us()`.)
**Фикс (применён):**
- deep-`now` = `rts_us()` — та же шкала, что у дедлайна `next` и у программирования сна.
- На старте (`startAsync`, power-on, при наличии deep-задач) — один короткий
  **калибровочный deep sleep** (2 мс) ДО первого user-дедлайна. На его wake (`mainLoop`, до
  любого user-огня) измеряется латентси `rts_us − calibIntendedWake` (~95–98 мс).
- Сетка всех deep-дедлайнов `deepTasksTimeFast` **сдвигается вниз** на измеренную латентси
  (переживает deep sleep через `deepTasks`). Тогда wake происходит на латентси раньше, а после
  загрузки задача встаёт **ровно в дедлайн** (±джиттер загрузки). Сдвиг ниже дедлайна держит
  задачу спелой после wake — без повторного deep sleep и пропусков (в отличие от более ранних
  попыток компенсировать сам сон: ранний wake → не спелая → re-deep-sleep → ребут → пропуск).
**Проверка:** `boot` (CORE0 2000/4000/6000/8000/10000, CORE1 2500/5000/7500/10000 — ровно с
первого огня; латентси 95–98 мс, ±десятки мкс между wake), `executor_deep` — то же.
**Цена:** `setup()` удлиняется на ~98 мс (калибровочный сон + его wake-boot).

---

## 5. Приоритет и стратегия исправления

**Порядок (критичные, крашатся в реальном использовании):**
1. **#11** — `1ULL` (один символ × 2).
2. **#2 + #13** — **сначала принять архитектурное решение**: единый мьютекс на
   `coreTasks`/`globalInterruptParams` (проще, но добавляет latency в `mainLoop`) vs
   lock-free очередь отложенных задач с единым потребителем (сложнее, без блокировок). Без
   этого фиксить вслепую нельзя.
3. **#3** — порядок re-arm в onDemand (ISR-безопасность #5 — IRAM-разметка — **исправлено**, см. запись).
4. **#8** — удалённый DoS (cap на тело/кадр).
5. Далее #6, #7, #9, #15 (жизненный цикл Active/Task), затем средние/низкие.

> **#1** (double-free в `~Task`), **#5** (ISR в IRAM + `gpio_ll_*`), **#14** (abort на длинный NVS-ключ),
> **#17** (`State::setIfChanged` + NVS-гард в `Setting`), **#21** (компенсация wake-латентси deep sleep) —
> **исправлены** (2026-07-30). **#6** — частично (слот регистрации). **#4 снят** — перепроверено, не баг:
> multi-pin pullup будится через ext1 `ALL_LOW`, поэтому будить одной кнопкой нельзя и
> «ложного срабатывания idle-пина» не возникает (см. запись #4).

**Как верифицировать фиксы:**
- Сборка: `pio run -e <env>` (по одному env на проблему, см. §0).
- #2/#3: `pin_active_interrupt` + дребезг/нагрузка NVS — отсутствие крашей. #5 проверен сборкой `pin_active_interrupt_flash`.
- #8: `http_simple` + curl с огромным `Content-Length`.
- После правок под S3 — временно собрать `_tmp` env `board = esp32-s3-devkitc-1` (не
  коммитить), см. заметку в §0 про `ASYNC_EXT1_WAKEUP_LOW` и `Uuid.h`.
