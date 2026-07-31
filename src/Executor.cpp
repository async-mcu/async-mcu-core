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
#include <async/Globals.h>

namespace async {

    // Globals планировщика определены inline в <async/Globals.h>.
    // sleepReadyMutex — file-local (static), используется только шедулером.
    static SemaphoreHandle_t sleepReadyMutex = NULL;

    static void mainLoop(void * parameter) {
        int core = (int) parameter;
        std::vector<Task *> toExecute;

        ESP_LOGV(TAG_EXECUTOR, "mainLoop from core %d curr time: %llu", core, rtcBoot);

        while (true) {
            uint64_t startCycleTime = esp_timer_get_time();
            uint64_t startCycleTimeRts = rts_us(); // deep: та же шкала, что у next и у программирования сна

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
        // if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
        //     rtsStartUs = rtsRawUs();
        // }

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

        // Восстановление таймингов Deep-задач из RTC — ТОЛЬКО при выходе из deep sleep.
        // На «настоящем» старте (power-on/soft-reset) deepTasksTime ещё неинициализирован
        // (== 0); безусловный возврат затирал бы первый дедлайн (тот, что регистрация
        // выставила в интервал) на 0 → задача срабатывала бы сразу при старте, а не
        // через интервал. На wake, наоборот, возврат корректен — продолжает шкалу.
        if (esp_reset_reason() == ESP_RST_DEEPSLEEP) {

            for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
                for (int i = 0; i < DEEP_TASKS_STACK; i++) {
                    deepTasksTimeFast[core][i] = deepTasksTime[core][i];
                    ESP_LOGI(TAG_EXECUTOR, "start deepTasksTimeFast[%d][%d] to %llu", core, i, deepTasksTime[core][i]);
                }

                for(auto & callback : afterWakeUpCallback[core]) {
                    callback(SleepMode::Deep);
                }
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
