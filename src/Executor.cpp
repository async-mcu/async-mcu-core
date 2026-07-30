#include <algorithm>
#include <atomic>
#include "sys/time.h"
#include "soc/rtc.h"
#include "driver/gptimer.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <async/Task.h>
#include <async/Executor.h>

namespace async {

    // Global variables definitions
    std::vector<Task *> coreTasks[SOC_CPU_CORES_NUM];
    TaskHandle_t coreTaskHandlers[SOC_CPU_CORES_NUM];
    std::vector<std::function<void(SleepMode)>> beforeEnterSleepCallback[SOC_CPU_CORES_NUM];
    std::vector<std::function<void(SleepMode)>> afterWakeUpCallback[SOC_CPU_CORES_NUM];

    RTC_DATA_ATTR uint64_t deepTasksTime[SOC_CPU_CORES_NUM][DEEP_TASKS_STACK];
    // База времени для rts_us()/rts_ms(): захватывается в initAsync() на старте,
    // переживает deep sleep → шкала непрерывна между пробуждениями.
    RTC_DATA_ATTR uint64_t rtsStartUs = 0;
    uint64_t deepTasksTimeFast[SOC_CPU_CORES_NUM][DEEP_TASKS_STACK];
    bool timerIsRunning = false;
    SleepMode sleepModeOverride = SleepMode::None;

    bool tickTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    bool lightTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    bool coreSleepReady[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    uint64_t minSleepTime[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);

    std::atomic<int> activeTasksCount = 0;
    uint64_t rtcBoot = 0;
    // Латентси загрузки после пробуждения из deep sleep (boot → старт цикла mainLoop).
    // Измеряется на калибровочном wake (см. startAsync) ОДИН раз; после этого сетка deep-дедлайнов
    // сдвигается вниз на эту величину, чтобы wake + загрузка приходились ровно на дедлайн.
    RTC_DATA_ATTR uint64_t deepWakeLatencyUs = 0;
    // Момент (шкала rts_us), в который должен был сработать калибровочный RTC-будок.
    // На calib-wake: rts_us(старт цикла) - calibIntendedWake = латентси загрузки.
    RTC_DATA_ATTR uint64_t calibIntendedWake = 0;

    // semaphore counting how many cores are ready to sleep
    static SemaphoreHandle_t sleepReadyMutex = NULL;

    // Global variable definitions needed from Interrupt.h

    // Сырое gettimeofday (мкс), без корректировки — нужно только для захвата базы старта.
    static int64_t rtsRawUs() {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    }

    // Время от старта планировщика (мкс). База rtsStartUs захватывается в initAsync()
    // на «настоящем» старте (power-on/soft-reset); при выходе из deep sleep сохраняется
    // (RTC_DATA_ATTR) → шкала непрерывна между пробуждениями, отсчёт начинается с ~0.
    int64_t rts_us() {
        return rtsRawUs() - rtsStartUs;
    }

    int64_t rts_ms() {
        return rts_us() / 1000ULL;
    }

    std::vector<Task *> getCoreTasks(Core core) {
        return coreTasks[core];
    }

    TaskHandle_t getCoreTaskHandler(Core core) {
        return coreTaskHandlers[core];
    }

    void checkDeepSleepTaskCanBeAdded() {
        if (isStarted()) {
            esp_system_abort("Deep tasks can only be added before the start() method.");
        }
    }

    void setSleepMode(SleepMode mode) {
        sleepModeOverride = mode;
    }

    SleepMode getSleepMode() {
        return sleepModeOverride;
    }

    void beforeEnterSleep(std::function<void(SleepMode)> callback) {
        beforeEnterSleepCallback[CURRENT_CORE].push_back(callback);
    }

    void afterWakeUp(std::function<void(SleepMode)> callback) {
        afterWakeUpCallback[CURRENT_CORE].push_back(callback);
    }

    void callTaskExecute(void *arg) {
        Task *task = (Task *)arg;
        task->execute();

        // одноразовая active-задача отстрелялась — снимаем с учёта
        if (task->getSleepMode() == SleepMode::Active && task->getType() == Type::DELAY && task->isCounted()) {
            activeTasksCount--;
            task->setCounted(false);
        }
    }

    void notifyActiveTaskCancelled(Task & task) {
        if (task.getSleepMode() == SleepMode::Active && task.isCounted()) {
            activeTasksCount--;
            task.setCounted(false);
        }
    }

    Task * onDelay(SleepMode sleepMode, Duration *delay, Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::DELAY, sleepMode, core, delay, callback);

        if (sleepMode == SleepMode::Active) {
            activeTasksCount++;
            task->setCounted(true);
            esp_timer_handle_t * _timer = new esp_timer_handle_t;
            *_timer = nullptr;
            esp_timer_create_args_t config = {
                .callback = callTaskExecute,
                .arg = task,
                .dispatch_method = ESP_TIMER_TASK,
                .skip_unhandled_events = false
            };

            ESP_ERROR_CHECK(esp_timer_create(&config, _timer));

            if (*_timer == nullptr) {
                esp_system_abort("Failed to create timer of active task");
            }

            ESP_ERROR_CHECK(esp_timer_start_once(*_timer, delay->us()));
            task->setTimer(_timer);
            task->setNext(task->getDelay());
        } else {
            if (sleepMode == SleepMode::Deep) {
                checkDeepSleepTaskCanBeAdded();
            } 
            else if (sleepMode == SleepMode::Light) {
                task->setNext(task->getDelay());
            }

            if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
                // Слот deepTasksTimeFast должен совпадать с ФИНАЛЬНОЙ позицией deep-задачи
                // в coreTasks: после удаления INIT-задач deep-задачи сдвигаются в начало.
                // Поэтому считаем уже зарегистрированные DEEP-задачи, а не весь размер
                // вектора — иначе INIT-задача в начале списка завышает idx и тайминг
                // попадает мимо слота (deep-задача читает 0 и срабатывает сразу при старте).
                size_t idx = 0;
                for (Task * t : coreTasks[core]) {
                    if (t->getSleepMode() == SleepMode::Deep) idx++;
                }
                if (idx < DEEP_TASKS_STACK) {
                    deepTasksTimeFast[core][idx] = task->getDelay();
                } else {
                    ESP_LOGE(TAG_EXECUTOR, "DEEP_TASKS_STACK (%d) exceeded on core %d", DEEP_TASKS_STACK, core);
                }
            }

            coreTasks[core].push_back(task);
        }

        return task;
    }

    Task * onRepeat(SleepMode sleepMode, Duration *interval, Duration *startDelay, Core core,
                    std::function<void(Task &)> callback) {
        auto task = new Task(Type::REPEAT, sleepMode, core, startDelay, interval, callback);

        if (sleepMode == SleepMode::Active) {
            activeTasksCount++;
            task->setCounted(true);
            esp_timer_handle_t * _timer = new esp_timer_handle_t;
            *_timer = nullptr;
            esp_timer_create_args_t config = {
                .callback = callTaskExecute,
                .arg = task,
                .dispatch_method = ESP_TIMER_TASK,
                .skip_unhandled_events = false
            };

            ESP_ERROR_CHECK(esp_timer_create(&config, _timer));

            if (*_timer == nullptr) {
                esp_system_abort("Failed to create timer of active task");
            }

            ESP_ERROR_CHECK(esp_timer_start_periodic(*_timer, interval->us()));
            task->setTimer(_timer);
            task->setNext(task->getDelay());
        } 
        else {
            if (sleepMode == SleepMode::Deep) {
                checkDeepSleepTaskCanBeAdded();
            } 
            else if (sleepMode == SleepMode::Light) {
                task->setNext(task->getDelay());
            }

            if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
                // Слот deepTasksTimeFast должен совпадать с ФИНАЛЬНОЙ позицией deep-задачи
                // в coreTasks: после удаления INIT-задач deep-задачи сдвигаются в начало.
                // Поэтому считаем уже зарегистрированные DEEP-задачи, а не весь размер
                // вектора — иначе INIT-задача в начале списка завышает idx и тайминг
                // попадает мимо слота (deep-задача читает 0 и срабатывает сразу при старте).
                size_t idx = 0;
                for (Task * t : coreTasks[core]) {
                    if (t->getSleepMode() == SleepMode::Deep) idx++;
                }
                if (idx < DEEP_TASKS_STACK) {
                    deepTasksTimeFast[core][idx] = task->getDelay();
                } else {
                    ESP_LOGE(TAG_EXECUTOR, "DEEP_TASKS_STACK (%d) exceeded on core %d", DEEP_TASKS_STACK, core);
                }
            }

            coreTasks[core].push_back(task);
        }

        return task;
    }

    Task * onDemand(Core core, std::function<void(Task &)> callback) {
        return new Task(Type::DEMAND, SleepMode::None, core, callback);
    }

    Task * onDemand(std::function<void(Task &)> callback) {
        return new Task(Type::DEMAND, SleepMode::None, CURRENT_CORE, callback);
    }

    Task * onOnce(Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::ONCE, SleepMode::Active, core, callback);
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onOnce(std::function<void(Task &)> callback) {
        return onOnce(CURRENT_CORE, callback);
    }

    Task * onOnce(Core core, Task * task) {
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onInit(Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::INIT, SleepMode::Active, core, callback);
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onTick(Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::TICK, SleepMode::Active, core, callback);
        task->setNext(0);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onTick(std::function<void(Task &)> callback) {
        return onTick(CURRENT_CORE, callback);
    }

    void mainLoop(void * parameter) {
        int core = (int) parameter;
        std::vector<Task *> toExecute;

        ESP_LOGV(TAG_EXECUTOR, "mainLoop from core %d curr time: %llu", core, rtcBoot);

        while (true) {
            uint64_t startCycleTime = esp_timer_get_time();
            uint64_t startCycleTimeRts = rts_us(); // deep: та же шкала, что у next и у программирования сна

            // Измерение латентси на калибровочном wake (ДО любого user-огня) + сдвиг сетки deep-дедлайнов.
            // Калибровочный wake создаёт короткий deep sleep в startAsync до первого user-дедлайна,
            // поэтому латентси известна заранее и ВСЕ user-огни (включая первый) попадают в дедлайн.
            if (deepWakeLatencyUs == 0 && calibIntendedWake != 0 && startCycleTimeRts > calibIntendedWake) {
                deepWakeLatencyUs = startCycleTimeRts - calibIntendedWake;
                for (int c = 0; c < SOC_CPU_CORES_NUM; c++) {
                    for (int s = 0; s < DEEP_TASKS_STACK; s++) {
                        if (deepTasksTimeFast[c][s] != UINT64_MAX && deepTasksTimeFast[c][s] > deepWakeLatencyUs) {
                            deepTasksTimeFast[c][s] -= deepWakeLatencyUs;
                        }
                    }
                }
                ESP_LOGI(TAG_EXECUTOR, "deep wake latency: %llu us (%llu ms) [cycleStart=%llu intendedWake=%llu] — grid shifted",
                         deepWakeLatencyUs, deepWakeLatencyUs / 1000, startCycleTimeRts, calibIntendedWake);
            }

            toExecute.clear();


            coreSleepReady[core] = false;
            bool localLightExists = false;
            bool localTickExists = false;
            uint64_t localMinSleep = UINT64_MAX;

            for (int i = 0; i < coreTasks[core].size(); i++) {
                if (coreTasks[core][i]->isCertainly()) {
                    toExecute.push_back(coreTasks[core][i]);
                    coreTasks[core][i]->setCertainly(false);

                    coreTasks[core].erase(coreTasks[core].begin() + i);
                    i--;
                } else if (coreTasks[core][i]->getType() == Type::TICK) {
                    if (coreTasks[core][i]->getNext() != UINT64_MAX) {
                        localTickExists = true;
                        toExecute.push_back(coreTasks[core][i]);
                    } else {
                        delete coreTasks[core][i];
                        coreTasks[core].erase(coreTasks[core].begin() + i);
                        i--;
                    }
                } 
                else {
                    bool isDeep = (coreTasks[core][i]->getSleepMode() == SleepMode::Deep);
                    bool deepSlotOk = isDeep && (i < DEEP_TASKS_STACK);
                    uint64_t next = coreTasks[core][i]->isCancelled()
                                        ? UINT64_MAX
                                        : (deepSlotOk ? deepTasksTimeFast[core][i] : coreTasks[core][i]->getNext());

                    ESP_LOGV(TAG_EXECUTOR, "PreTask %d, next: %llu, interval: %llu", i, next, coreTasks[core][i]->getType() == Type::REPEAT ? (uint64_t) coreTasks[core][i]->getInterval() : 0ULL);

                    // Ожидающая light-задача удерживает ядро в режиме light sleep.
                    // Иначе, если ядро долго бодрствовало из-за active/tick-задач и
                    // light-задача не успела сработать в этой итерации, планировщик
                    // «забывает» про неё и уходит в deep sleep, который стирает её из RAM.
                    if (coreTasks[core][i]->getSleepMode() == SleepMode::Light && next != UINT64_MAX) {
                        localLightExists = true;
                    }

                    if (next != UINT64_MAX) {
                        uint64_t now = isDeep ? startCycleTimeRts : startCycleTime;
                    
                        ESP_LOGV(TAG_EXECUTOR, "StartTask %d, now: %llu", i, now);

                        if (now >= next) {
                            toExecute.push_back(coreTasks[core][i]);

                            if (coreTasks[core][i]->getType() == Type::REPEAT) {
                                if(deepSlotOk) {
                                    deepTasksTimeFast[core][i] += coreTasks[core][i]->getInterval();
                                }
                                else {
                                    localLightExists = true;
                                    coreTasks[core][i]->setNext(coreTasks[core][i]->getNext() + coreTasks[core][i]->getInterval());
                                }
                            } else {
                                if(deepSlotOk) {
                                    deepTasksTimeFast[core][i] = UINT64_MAX;
                                }
                                else {
                                    coreTasks[core][i]->setNext(UINT64_MAX);
                                }
                            }
                        }
                        else if (!localTickExists) {
                            localMinSleep = localMinSleep < next
                                                      ? localMinSleep
                                                      : next;
                        }

                        ESP_LOGV(TAG_EXECUTOR, "PostTask %d, next: %llu, minSleepTime: %llu", i, next, localMinSleep);
                    }
                    else if(coreTasks[core][i]->getSleepMode() == SleepMode::Light) {
                        delete coreTasks[core][i];
                        coreTasks[core].erase(coreTasks[core].begin() + i);
                        i--;
                    }
                }
            }

            // Публикуем посчитанное per-core состояние одним записыванием, пока
            // coreSleepReady[core] == false. Тогда ядро, захватившее мьютекс,
            // читает согласованный снимок — нет гонки с перевычислением соседним ядром.
            lightTasksExists[core] = localLightExists;
            tickTasksExists[core] = localTickExists;
            minSleepTime[core] = localMinSleep;

            if (toExecute.size() > 0) {
                coreSleepReady[core] = false;

                for (Task * task : toExecute) {
                    task->execute();
                    if (task->getType() == Type::ONCE) {
                        delete task;
                    }
                }

                vTaskDelay(1);
                continue;
            } else if (BOOL_OR(tickTasksExists, SOC_CPU_CORES_NUM) || activeTasksCount.load() > 0 || getInterruptSleepMode() == SleepMode::Active || sleepModeOverride == SleepMode::Active) {
                coreSleepReady[core] = false;
                vTaskDelay(1);
                continue;
            }

            coreSleepReady[core] = true;

            if (BOOL_AND(coreSleepReady, SOC_CPU_CORES_NUM) && xSemaphoreTake(sleepReadyMutex, 0) == pdTRUE) {
                    bool blockDeepSleepByInterrupt = false;

                    //if (getDeepInterruptsRevert()) {
                        int revertedPinsCount = 0;
                        uint64_t pinsMask = 0;

                        for (auto interruptParam : getGlobalInterruptParams()) {
                            if (interruptParam->sleepMode == SleepMode::Deep && !(pinsMask & (1 << interruptParam->pin.getPin()))) {
                                ESP_LOGV(TAG_EXECUTOR, "add deep sleep pin: %d", interruptParam->pin.getPin());
                                pinsMask |= (1ULL << interruptParam->pin.getPin());

                                if (interruptParam->pin.isReverted()) {
                                    revertedPinsCount++;
                                }
                            }
                        }
                        
                        ESP_LOGV(TAG_EXECUTOR, "pinsMaskCount %d, revertedPinsCount %d", __builtin_popcountll(pinsMask), revertedPinsCount);

                        if (__builtin_popcountll(pinsMask) > 1 && revertedPinsCount > 0) {
                            blockDeepSleepByInterrupt = true;
                        } else if (revertedPinsCount == 0) {
                            setDeepInterruptsRevert(false);
                        }
                    //}

                    // light sleep
                    if (blockDeepSleepByInterrupt || BOOL_OR(lightTasksExists, SOC_CPU_CORES_NUM) || getInterruptSleepMode() == SleepMode::Light || sleepModeOverride == SleepMode::Light) {
                        uint64_t finalMinSleepTime = minInArray(minSleepTime, SOC_CPU_CORES_NUM);
                        
                        if (timerIsRunning) {
                            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                            timerIsRunning = false;
                        }

                        if (blockDeepSleepByInterrupt || getInterruptSleepMode() == SleepMode::Light) {
                            esp_sleep_enable_gpio_wakeup();
                        }

                        ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, light sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                        ESP_LOGI(TAG_EXECUTOR, "esp_light_sleep_start from core %d sleep time: %llu", core, finalMinSleepTime - esp_timer_get_time());
                        
                        uint64_t finalLightSleepInterval = finalMinSleepTime - esp_timer_get_time();
                        for (int core_num = 0; core_num < SOC_CPU_CORES_NUM; core_num++) {
                            for(auto & callback : beforeEnterSleepCallback[core_num]) {
                                callback(SleepMode::Light);
                            }
                        }

                        if (finalMinSleepTime != UINT64_MAX) {
                            esp_sleep_enable_timer_wakeup(finalMinSleepTime - esp_timer_get_time());
                            timerIsRunning = true;
                        }

                        esp_light_sleep_start();

                        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
                        ESP_LOGD(TAG_EXECUTOR, "esp_sleep_get_wakeup_cause: %d", cause);

                        for (int core_num = 0; core_num < SOC_CPU_CORES_NUM; core_num++) {
                            coreSleepReady[core_num] = false;
                            for(auto & callback : afterWakeUpCallback[core_num]) {
                                callback(SleepMode::Light);
                            }
                        }
                    }
                    // deep sleep
                    else {
                        for (int core_upd = 0; core_upd < SOC_CPU_CORES_NUM; core_upd++) {
                            for (int i = 0; i < DEEP_TASKS_STACK; i++) {
                                deepTasksTime[core_upd][i] = deepTasksTimeFast[core_upd][i];
                            }
                        }

                        ESP_LOGV(TAG_EXECUTOR, "get deepInterruptsRevert = %d", getDeepInterruptsRevert());

                        uint64_t pinsMask = 0;
                        for (auto interruptParam : getGlobalInterruptParams()) {
                            if (interruptParam->sleepMode == SleepMode::Deep && !(pinsMask & (1 << interruptParam->pin.getPin()))) {
                                ESP_LOGV(TAG_EXECUTOR, "add deep sleep pin: %d", interruptParam->pin.getPin());
                                pinsMask |= (1ULL << interruptParam->pin.getPin());
                            }
                        }

                        int countPins = __builtin_popcountll(pinsMask);

                        if (countPins == 1) {
                            if (!getDeepInterruptsRevert()) {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext0_wakeup pin %llu to %d", __builtin_ctzll(pinsMask), getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                                esp_sleep_enable_ext0_wakeup((gpio_num_t) __builtin_ctzll(pinsMask), getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                            } else {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext0_wakeup pin %llu revert to %d", __builtin_ctzll(pinsMask), getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                                esp_sleep_enable_ext0_wakeup((gpio_num_t) __builtin_ctzll(pinsMask), getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                            }
                        } else if (countPins > 1) {
                            if (!getDeepInterruptsRevert()) {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext1_wakeup mask %llu to %d", pinsMask, getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                                esp_sleep_enable_ext1_wakeup_io(pinsMask, getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                            } else {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext1_wakeup mask %llu revert to %d", pinsMask, getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                                esp_sleep_enable_ext1_wakeup_io(pinsMask, getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ASYNC_EXT1_WAKEUP_LOW);
                            }
                        }

                        ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, deep sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                        ESP_LOGI(TAG_EXECUTOR, "esp_deep_sleep_start from core %d sleep time: %llu", core, minInArray(minSleepTime, SOC_CPU_CORES_NUM) - rts_us());
                        
                        for (int core_num = 0; core_num < SOC_CPU_CORES_NUM; core_num++) {
                            for(auto & callback : beforeEnterSleepCallback[core_num]) {
                                callback(SleepMode::Deep);
                            }
                        }

                        if (minInArray(minSleepTime, SOC_CPU_CORES_NUM) != UINT64_MAX) {
                            esp_sleep_enable_timer_wakeup(minInArray(minSleepTime, SOC_CPU_CORES_NUM) - rts_us());
                        }

                        esp_deep_sleep_start();
                    }
                xSemaphoreGive(sleepReadyMutex);
            }
        }
    }

    void initAsync() {
        // Включаем INFO-логи планировщика — чтобы были видны стартовый баннер и решения о сне.
        esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO);

        // База времени: на «настоящем» старте отсчитываем заново (rts_us()/rts_ms() с ~0).
        // При пробуждении из deep sleep база сохраняется — шкала остаётся непрерывной.
        if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
            rtsStartUs = rtsRawUs();
        }

        for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
            for (int i = 0; i < coreTasks[core].size(); i++) {
                if (coreTasks[core][i]->getType() == INIT) {
                    coreTasks[core][i]->execute();
                    delete coreTasks[core][i];
                    coreTasks[core].erase(coreTasks[core].begin() + i);
                    i--;
                }
            }
        }
    }

    void startAsync() {
        if (isStarted()) {
            esp_system_abort("Already started");
        }
        setStarted(true);

        // Метка «настоящего» старта (power-on / soft-reset); на выходе из deep sleep не логируем.
        if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
            ESP_LOGI(TAG_EXECUTOR, ">>> Async started (reset_reason=%d)", (int) esp_reset_reason());
            vTaskDelay(pdMS_TO_TICKS(10)); // дать UART сбросить лог до возможного немедленного deep sleep
        }

        // Каждая Deep-задача должна попадать в первые DEEP_TASKS_STACK слотов,
        // иначе её тайминг не переживёт deep sleep и она не выполнится.
        for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
            for (size_t i = 0; i < coreTasks[core].size(); i++) {
                if (coreTasks[core][i]->getSleepMode() == SleepMode::Deep && (int) i >= DEEP_TASKS_STACK) {
                    ESP_LOGE(TAG_EXECUTOR, "Deep task at index %zu on core %d exceeds DEEP_TASKS_STACK (%d)", i, core, DEEP_TASKS_STACK);
                    esp_system_abort("startAsync: Deep task count exceeds DEEP_TASKS_STACK");
                }
            }
        }

        rtcBoot = rts_us();
        sleepReadyMutex = xSemaphoreCreateMutex();

        for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
            // Восстановление таймингов Deep-задач из RTC — ТОЛЬКО при выходе из deep sleep.
            // На «настоящем» старте (power-on/soft-reset) deepTasksTime ещё неинициализирован
            // (== 0); безусловный возврат затирал бы первый дедлайн (тот, что регистрация
            // выставила в интервал) на 0 → задача срабатывала бы сразу при старте, а не
            // через интервал. На wake, наоборот, возврат корректен — продолжает шкалу.
            if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {
                for (int i = 0; i < DEEP_TASKS_STACK; i++) {
                    deepTasksTimeFast[core][i] = deepTasksTime[core][i];
                    ESP_LOGI(TAG_EXECUTOR, "start deepTasksTimeFast[%d][%d] to %llu", core, i, deepTasksTime[core][i]);
                }

                for(auto & callback : afterWakeUpCallback[core]) {
                    callback(SleepMode::Deep);
                }
            }
        }

        // Калибровка латентси deep-wake на «настоящем» старте: короткий deep sleep ДО первого
        // user-дедлайна. На его wake (в mainLoop) измеряем латентси и сдвигаем сетку заранее —
        // тогда ВСЕ user-огни (включая первый) попадают точно в дедлайн, без калибровочного огня.
        if (esp_reset_reason() != ESP_RST_DEEPSLEEP && deepWakeLatencyUs == 0 && calibIntendedWake == 0) {
            bool hasDeep = false;
            for (int c = 0; c < SOC_CPU_CORES_NUM && !hasDeep; c++) {
                for (Task * t : coreTasks[c]) {
                    if (t->getSleepMode() == SleepMode::Deep) { hasDeep = true; break; }
                }
            }
            if (hasDeep) {
                for (int c = 0; c < SOC_CPU_CORES_NUM; c++) {
                    for (int s = 0; s < DEEP_TASKS_STACK; s++) {
                        deepTasksTime[c][s] = deepTasksTimeFast[c][s]; // сохранить для restore на calib-wake
                    }
                }
                calibIntendedWake = rts_us() + 2000ULL; // 2 мс калибровочного сна
                esp_sleep_enable_timer_wakeup(2000ULL);
                esp_deep_sleep_start(); // не вернётся — wake стартует новый boot (setup → startAsync)
            }
        }

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) {
            setDeepInterruptsRevert(!getDeepInterruptsRevert());
        }

        for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
            xTaskCreatePinnedToCore(mainLoop,
                                    "",
                                    10000,
                                    (void *) core,
                                    1,
                                    &coreTaskHandlers[core],
                                    core);
        }
    }
}