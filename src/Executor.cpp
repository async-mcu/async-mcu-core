#include <algorithm>
#include "sys/time.h"
#include "soc/rtc.h"
#include "driver/gptimer.h"
#include "esp_event.h"
#include "esp_timer.h"

#include <async/Task.h>
#include <async/Executor.h>

namespace async {

    // Global variables definitions
    std::vector<Task *> coreTasks[SOC_CPU_CORES_NUM];
    TaskHandle_t coreTaskHandlers[SOC_CPU_CORES_NUM];

    RTC_DATA_ATTR uint64_t deepTasksTime[DEEP_TASKS_STACK];
    uint64_t deepTasksTimeFast[DEEP_TASKS_STACK];
    bool timerIsRunning = false;

    bool tickTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    bool lightTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    bool coreSleepReady[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    uint64_t minSleepTimeDeep[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);
    uint64_t minSleepTimeLight[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);
    volatile bool goToSleep = false;

    int activeTasksCount = 0;
    uint64_t rtcBoot = 0;

    // Global variable definitions needed from Interrupt.h
    int64_t rts_us() {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
        return time_us;
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

    void callTaskExecute(void *arg) {
        Task *task = (Task *)task;
        task->execute();

        if (task->getSleepMode() == SleepMode::Active && task->getType() == Type::DELAY) {
            activeTasksCount--;
        }
    }

    Task * onDelay(SleepMode sleepMode, Duration *delay, Core core, std::function<void(Task *)> callback) {
        auto task = new Task(Type::DELAY, sleepMode, core, delay, callback);

        if (sleepMode == SleepMode::Active) {
            activeTasksCount++;
            static esp_timer_handle_t _timer = NULL;
            esp_timer_create_args_t config = {
                .callback = callTaskExecute,
                .arg = task,
                .dispatch_method = ESP_TIMER_TASK,
                .skip_unhandled_events = false
            };

            ESP_ERROR_CHECK(esp_timer_create(&config, &_timer));

            if (_timer == NULL) {
                esp_system_abort("Failed to create timer of active task");
            }

            ESP_ERROR_CHECK(esp_timer_start_once(_timer, delay->us()));
            task->setTimer(&_timer);
            task->setNext(task->getDelay());
        } else {
            if (sleepMode == SleepMode::Deep) {
                checkDeepSleepTaskCanBeAdded();
            } 
            else if (sleepMode == SleepMode::Light) {
                task->setNext(task->getDelay());
            }

            if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
                deepTasksTime[coreTasks[core].size()] = task->getDelay();
            }

            coreTasks[core].push_back(task);
        }

        return task;
    }

    Task * onRepeat(SleepMode sleepMode, Duration *interval, Duration *startDelay, Core core,
                    std::function<void(Task *)> callback) {
        auto task = new Task(Type::REPEAT, sleepMode, core, startDelay, interval, callback);

        if (sleepMode == SleepMode::Active) {
            activeTasksCount++;
            static esp_timer_handle_t _timer = NULL;
            esp_timer_create_args_t config = {
                .callback = callTaskExecute,
                .arg = task,
                .dispatch_method = ESP_TIMER_TASK,
                .skip_unhandled_events = false
            };

            ESP_ERROR_CHECK(esp_timer_create(&config, &_timer));

            if (_timer == NULL) {
                esp_system_abort("Failed to create timer of active task");
            }

            ESP_ERROR_CHECK(esp_timer_start_periodic(_timer, interval->us()));
            task->setTimer(&_timer);
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
                deepTasksTime[coreTasks[core].size()] = task->getDelay();
            }

            coreTasks[core].push_back(task);
        }

        return task;
    }

    Task * onDemand(Core core, std::function<void(Task *)> callback) {
        return new Task(Type::DEMAND, SleepMode::None, core, callback);
    }

    Task * onDemand(std::function<void(Task *)> callback) {
        return new Task(Type::DEMAND, SleepMode::None, CURRENT_CORE, callback);
    }

    Task * onOnce(Core core, std::function<void(Task *)> callback) {
        auto task = new Task(Type::ONCE, SleepMode::Active, core, callback);
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onOnce(std::function<void(Task *)> callback) {
        return onOnce(CURRENT_CORE, callback);
    }

    Task * onOnce(Core core, Task * task) {
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onInit(Core core, std::function<void(Task *)> callback) {
        auto task = new Task(Type::INIT, SleepMode::Active, core, callback);
        task->setCertainly(true);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onTick(Core core, std::function<void(Task *)> callback) {
        auto task = new Task(Type::TICK, SleepMode::Active, core, callback);
        task->setNext(0);
        coreTasks[core].push_back(task);
        return task;
    }

    Task * onTick(std::function<void(Task *)> callback) {
        return onTick(CURRENT_CORE, callback);
    }

    void mainLoop(void * parameter) {
        int core = (int) parameter;
        std::vector<Task *> toExecute;

        ESP_LOGV(TAG_EXECUTOR, "mainLoop from core %d curr time: %llu", core, minSleepTimeLight[core]);

        while (true) {
            uint64_t startCycleTime = esp_timer_get_time();
            uint64_t startCycleTimeWithRtc = rtcBoot + startCycleTime;

            toExecute.clear();

            lightTasksExists[core] = false;
            tickTasksExists[core] = false;
            minSleepTimeLight[core] = UINT64_MAX;
            minSleepTimeDeep[core] = UINT64_MAX;

            for (int i = 0; i < coreTasks[core].size(); i++) {
                if (coreTasks[core][i]->isCertainly()) {
                    toExecute.push_back(coreTasks[core][i]);
                    coreTasks[core][i]->setCertainly(false);

                    if (coreTasks[core][i]->getType() == Type::ONCE) {
                        delete coreTasks[core][i];
                    }

                    coreTasks[core].erase(coreTasks[core].begin() + i);
                    i--;
                } else if (coreTasks[core][i]->getType() == Type::TICK) {
                    if (coreTasks[core][i]->getNext() != UINT64_MAX) {
                        tickTasksExists[core] = true;
                        toExecute.push_back(coreTasks[core][i]);
                    } else {
                        delete coreTasks[core][i];
                        coreTasks[core].erase(coreTasks[core].begin() + i);
                        i--;
                    }
                } else if (coreTasks[core][i]->getSleepMode() == SleepMode::Light) {
                    if (coreTasks[core][i]->getNext() != UINT64_MAX) {
                        lightTasksExists[core] = true;

                        if (startCycleTime >= coreTasks[core][i]->getNext()) {
                            toExecute.push_back(coreTasks[core][i]);

                            if (coreTasks[core][i]->getType() == Type::REPEAT) {
                                coreTasks[core][i]->setNext(coreTasks[core][i]->getNext() + coreTasks[core][i]->getInterval());
                            } else {
                                coreTasks[core][i]->setNext(UINT64_MAX);
                            }
                        } else if (!tickTasksExists[core]) {
                            minSleepTimeLight[core] = minSleepTimeLight[core] < coreTasks[core][i]->getNext()
                                                      ? minSleepTimeLight[core]
                                                      : coreTasks[core][i]->getNext();
                        }
                    } else {
                        delete coreTasks[core][i];
                        coreTasks[core].erase(coreTasks[core].begin() + i);
                        i--;
                    }
                } else if (coreTasks[core][i]->getSleepMode() == SleepMode::Deep) {
                    if (deepTasksTimeFast[i] != UINT64_MAX) {
                        if (startCycleTimeWithRtc >= deepTasksTimeFast[i]) {
                            toExecute.push_back(coreTasks[core][i]);

                            if (coreTasks[core][i]->getType() == Type::REPEAT) {
                                deepTasksTimeFast[i] += coreTasks[core][i]->getInterval();
                            } else {
                                deepTasksTimeFast[i] = UINT64_MAX;
                            }
                        } else if (!tickTasksExists[core] && !lightTasksExists[core]) {
                            minSleepTimeDeep[core] = minSleepTimeDeep[core] < deepTasksTimeFast[i]
                                                     ? minSleepTimeDeep[core]
                                                     : deepTasksTimeFast[i];
                        }
                    }
                }
            }

            const bool tickTasksExistsFinal = BOOL_OR(tickTasksExists, SOC_CPU_CORES_NUM);
            const bool lightTasksExistsFinal = BOOL_OR(lightTasksExists, SOC_CPU_CORES_NUM);
            const bool coreSleepReadyFinal = BOOL_AND(coreSleepReady, SOC_CPU_CORES_NUM);

            if (toExecute.size() > 0) {
                coreSleepReady[core] = false;

                for (Task * task : toExecute) {
                    task->execute();
                }

                continue;
            } else if (tickTasksExistsFinal || activeTasksCount > 0 || getInterruptSleepMode() == SleepMode::Active) {
                continue;
            }

            if(!goToSleep) {
                if(coreSleepReadyFinal) {
                    goToSleep = true;

                    bool blockDeepSleepByInterrupt = false;

                    //if (getDeepInterruptsRevert()) {
                        int revertedPinsCount = 0;
                        uint64_t pinsMask = 0;

                        for (auto interruptParam : getGlobalInterruptParams()) {
                            if (interruptParam->sleepMode == SleepMode::Deep && !(pinsMask & (1 << interruptParam->pin->getPin()))) {
                                ESP_LOGV(TAG_EXECUTOR, "add deep sleep pin: %d", interruptParam->pin->getPin());
                                pinsMask |= (1ULL << interruptParam->pin->getPin());

                                if (interruptParam->pin->isReverted()) {
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
                    if (blockDeepSleepByInterrupt || lightTasksExistsFinal || getInterruptSleepMode() == SleepMode::Light) {
                        if (MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) != UINT64_MAX) {
                            esp_sleep_enable_timer_wakeup(MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) - esp_timer_get_time());
                            timerIsRunning = true;
                        } else if (timerIsRunning) {
                            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                            timerIsRunning = false;
                        }

                        if (blockDeepSleepByInterrupt || getInterruptSleepMode() == SleepMode::Light) {
                            esp_sleep_enable_gpio_wakeup();
                        }

                        ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, light sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                        ESP_LOGD(TAG_EXECUTOR, "esp_light_sleep_start from core %d sleep time: %llu", core, MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) - esp_timer_get_time());
                        
                        fflush(stdout);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        esp_light_sleep_start();

                        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
                        ESP_LOGV(TAG_EXECUTOR, "esp_sleep_get_wakeup_cause: %d", cause);

                        coreSleepReady[core] = false;
                        goToSleep = false;
                    }
                    // deep sleep
                    else {
                        for (int i = 0; i < DEEP_TASKS_STACK; i++) {
                            deepTasksTimeFast[i] = deepTasksTime[i];
                        }

                        if (MIN_IN_ARRAY(minSleepTimeDeep, SOC_CPU_CORES_NUM) != UINT64_MAX) {
                            esp_sleep_enable_timer_wakeup(MIN_IN_ARRAY(minSleepTimeDeep, SOC_CPU_CORES_NUM) - rts_us());
                            timerIsRunning = true;
                        } else if (timerIsRunning) {
                            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                            timerIsRunning = false;
                        }

                        ESP_LOGV(TAG_EXECUTOR, "get deepInterruptsRevert = %d", getDeepInterruptsRevert());

                        uint64_t pinsMask = 0;
                        for (auto interruptParam : getGlobalInterruptParams()) {
                            if (interruptParam->sleepMode == SleepMode::Deep && !(pinsMask & (1 << interruptParam->pin->getPin()))) {
                                ESP_LOGV(TAG_EXECUTOR, "add deep sleep pin: %d", interruptParam->pin->getPin());
                                pinsMask |= (1ULL << interruptParam->pin->getPin());
                            }
                        }

                        int countPins = __builtin_popcountll(pinsMask);

                        if (countPins == 1) {
                            if (!getDeepInterruptsRevert()) {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext0_wakeup pin %llu to %d", __builtin_ctzll(pinsMask), getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                                esp_sleep_enable_ext0_wakeup((gpio_num_t) __builtin_ctzll(pinsMask), getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                            } else {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext0_wakeup pin %llu revert to %d", __builtin_ctzll(pinsMask), getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                                esp_sleep_enable_ext0_wakeup((gpio_num_t) __builtin_ctzll(pinsMask), getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                            }
                        } else if (countPins > 1) {
                            if (!getDeepInterruptsRevert()) {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext1_wakeup mask %llu to %d", pinsMask, getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                                esp_sleep_enable_ext1_wakeup_io(pinsMask, getDeepInterruptsMode() == INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                            } else {
                                ESP_LOGD(TAG_EXECUTOR, "esp_sleep_enable_ext1_wakeup mask %llu revert to %d", pinsMask, getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                                esp_sleep_enable_ext1_wakeup_io(pinsMask, getDeepInterruptsMode() != INPUT_PULLDOWN ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
                            }
                        }

                        ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, deep sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                        ESP_LOGD(TAG_EXECUTOR, "esp_deep_sleep_start from core %d sleep time: %llu", core, MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) - esp_timer_get_time());
                        fflush(stdout);
                        vTaskDelay(pdMS_TO_TICKS(100));

                        esp_deep_sleep_start();
                    }

                }
                else {
                    coreSleepReady[core] = true;
                }
            }

            vTaskDelay(1);
        }
    }

    void init() {
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

    void start() {
        if (isStarted()) {
            esp_system_abort("Already started");
        }
        setStarted(true);

        rtcBoot = rts_us();

        for (int i = 0; i < DEEP_TASKS_STACK; i++) {
            deepTasksTimeFast[i] = deepTasksTime[i];
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