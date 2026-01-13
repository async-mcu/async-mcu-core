#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include "esp_sleep.h"
#include "driver/gptimer.h"
#include "soc/rtc.h"
#include "esp_system.h"
#include "esp_private/panic_internal.h"
#include "esp_heap_trace.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event.h"
#include "sys/time.h"
#include <async/Duration.h>
#include <async/Task.h>
#include <async/Interrupts.h>
#include <async/Logging.h>

#define CURRENT_CORE (esp_cpu_get_core_id() == 0 ? CORE0 : CORE1)
#define DEEP_TASKS_STACK 50

namespace async {
std::vector<Task *> tasks[2];

RTC_DATA_ATTR uint64_t deepTasksTime[DEEP_TASKS_STACK]; // agata call dad
              uint64_t deepTasksTimeFast[DEEP_TASKS_STACK]; // agata call dad
bool startFlag = false;
bool timerIsRunning = false;


bool tickTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
bool lightTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
bool coreSleepReady[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
uint64_t minSleepTimeDeep[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);
uint64_t minSleepTimeLight[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);
volatile bool goToSleep = false;

int activeTasksCount = 0;
uint64_t rtcBoot = 0;
TaskHandle_t taskLoopCore0;
TaskHandle_t taskLoopCore1;

extern std::vector<interrupt_params> interrupts;

/**
 * @brief Get the current real-time stamp in microseconds.
 * 
 * This function retrieves the current time in microseconds using gettimeofday.
 * Note: This is a slow operation.
 * 
 * @return int64_t Current time in microseconds.
 */
int64_t rts_us() { // slow
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    return time_us;
}

/**
 * @brief Get the current real-time stamp in milliseconds.
 * 
 * This function retrieves the current time in milliseconds by dividing microseconds by 1000.
 * 
 * @return int64_t Current time in milliseconds.
 */
int64_t rts_ms() {
    return rts_us()/1000ULL;
}

/**
 * @brief Check if deep sleep tasks can be added.
 * 
 * This function verifies that the executor has not been started yet, as deep sleep tasks
 * can only be added before the start() method is called.
 * 
 * @throws esp_system_abort if the executor has already been started.
 */
void checkDeepSleepTaskCanBeAdded() {
    if(startFlag) {
        esp_system_abort("Deep tasks can only be added before the start() method.");
    }
}

/**
 * @brief Static callback function to execute a task.
 * 
 * This function is used as a callback for ESP timers to execute tasks.
 * It calls the task's execute method and decrements the active tasks count
 * for active delay tasks.
 * 
 * @param arg Pointer to the Task object to execute (cast from void*).
 */
static void callTaskExecute(void *arg) {
    Task *obj = (Task *)arg;
    obj->execute();

    if(obj->getMode() == Mode::Active && obj->getType() == Type::DELAY) {
        activeTasksCount--;
    }
}

/**
 * @brief Schedule a task to execute after a specified delay.
 * 
 * Creates a delay task that will execute once after the specified delay.
 * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
 * 
 * @tparam mode The execution mode (Active, Light, or Deep).
 * @param delay The duration to wait before executing the task.
 * @param core The CPU core to run the task on.
 * @param callback The function to execute when the delay expires.
 * @return Task* Pointer to the created task.
 */
template<Mode mode>
Task * onDelay(Duration * delay, Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::DELAY, mode, core, delay, callback);

    if(mode == Mode::Active) {
        activeTasksCount++;
        static esp_timer_handle_t _timer = NULL;
        // Настраиваем параметры таймера
        esp_timer_create_args_t config = {
            .callback = callTaskExecute,
            .arg = task,
            .dispatch_method = ESP_TIMER_TASK, // Обработчик будет вызван из задачи (другой вариант - из ISR)
            .skip_unhandled_events = false     // Если какое-либо срабатывание таймера было пропущено, то обработать его в любом случае
        }; 

        ESP_ERROR_CHECK(esp_timer_create(&config, &_timer));

        if (_timer == NULL) {
            esp_system_abort("Failed to create timer of active task");
        };

        ESP_ERROR_CHECK(esp_timer_start_once(_timer, delay->us()));
        task->setTimer(&_timer);
        task->setNext(task->getDelay());
    }
    else {
        if (mode == Mode::Deep) {
            checkDeepSleepTaskCanBeAdded();
        }
        else if(mode == Mode::Light) {
            task->setNext(task->getDelay());
        }

        if(esp_reset_reason() != ESP_RST_DEEPSLEEP) {
            deepTasksTime[tasks[core].size()] = task->getDelay();
        }

        tasks[core].push_back(task);
    }

    return task;
}

/**
 * @brief Schedule a task to execute after a specified delay on the current core.
 * 
 * Creates a delay task that will execute once after the specified delay on the current CPU core.
 * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
 * 
 * @tparam mode The execution mode (Active, Light, or Deep).
 * @param delay The duration to wait before executing the task.
 * @param callback The function to execute when the delay expires.
 * @return Task* Pointer to the created task.
 */
template<Mode mode>
Task * onDelay(Duration * delay, std::function<void(Task *)> callback) {
    return onDelay<mode>(delay, CURRENT_CORE, callback);
}

/**
 * @brief Schedule a task to execute repeatedly with a specified interval and initial delay.
 * 
 * Creates a repeating task that will first execute after startDelay, then repeat every interval.
 * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
 * 
 * @tparam mode The execution mode (Active, Light, or Deep).
 * @param interval The interval between executions.
 * @param startDelay The initial delay before first execution.
 * @param core The CPU core to run the task on.
 * @param callback The function to execute at each interval.
 * @return Task* Pointer to the created task.
 */
template<Mode mode> 
Task * onRepeat(Duration * interval, Duration * startDelay, Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::REPEAT, mode, core, startDelay, interval, callback);

    if (mode == Mode::Active) {
        activeTasksCount++;
        static esp_timer_handle_t _timer = NULL;
        // Настраиваем параметры таймера
        esp_timer_create_args_t config = {
            .callback = callTaskExecute,
            .arg = task,
            .dispatch_method = ESP_TIMER_TASK, // Обработчик будет вызван из задачи (другой вариант - из ISR)
            .skip_unhandled_events = false     // Если какое-либо срабатывание таймера было пропущено, то обработать его в любом случае
        }; 

        ESP_ERROR_CHECK(esp_timer_create(&config, &_timer));

        if (_timer == NULL) {
            esp_system_abort("Failed to create timer of active task");
        };
 
        ESP_ERROR_CHECK(esp_timer_start_periodic(_timer, interval->us()));
        task->setTimer(&_timer);
        task->setNext(task->getDelay());
    }
    else {
        if (mode == Mode::Deep) {
            checkDeepSleepTaskCanBeAdded();
        }
        else if(mode == Mode::Light) {
            task->setNext(task->getDelay());
        }
        
        if(esp_reset_reason() != ESP_RST_DEEPSLEEP) {
            deepTasksTime[tasks[core].size()] = task->getDelay();
        }

        tasks[core].push_back(task);
    }

    return task;
}

/**
 * @brief Schedule a task to execute repeatedly with a specified interval.
 * 
 * Creates a repeating task that will execute every interval period on the specified core.
 * The first execution happens after the interval duration.
 * 
 * @tparam mode The execution mode (Active, Light, or Deep).
 * @param interval The interval between executions (also used as start delay).
 * @param core The CPU core to run the task on.
 * @param callback The function to execute at each interval.
 * @return Task* Pointer to the created task.
 */
template<Mode mode> 
Task * onRepeat(Duration * interval, Core core, std::function<void(Task *)> callback) {
    return onRepeat<mode>(interval, interval, core, callback);
}

/**
 * @brief Schedule a task to execute repeatedly with a specified interval on the current core.
 * 
 * Creates a repeating task that will execute every interval period on the current CPU core.
 * The first execution happens after the interval duration.
 * 
 * @tparam mode The execution mode (Active, Light, or Deep).
 * @param interval The interval between executions (also used as start delay).
 * @param callback The function to execute at each interval.
 * @return Task* Pointer to the created task.
 */
template<Mode mode> 
Task * onRepeat(Duration * interval, std::function<void(Task *)> callback) {
    return onRepeat<mode>(interval, interval, CURRENT_CORE, callback);
}

/**
 * @brief Create an on-demand task that executes when manually triggered.
 * 
 * Creates a task that will only execute when explicitly called. This is useful for
 * tasks that need to be triggered by external events or conditions.
 * 
 * @param core The CPU core to run the task on.
 * @param callback The function to execute when the task is triggered.
 * @return Task* Pointer to the created task.
 */
Task * onDemand(Core core, std::function<void(Task *)> callback) {
    return new Task(Type::DEMAND, Mode::None, core, callback);
}

Task * onDemand(std::function<void(Task *)> callback) {
    return new Task(Type::DEMAND, Mode::None, CURRENT_CORE, callback);
}

/**
 * @brief Schedule a task to execute exactly once.
 * 
 * Creates a task that will execute only once and then be automatically deleted.
 * The task is added to the task list and will be executed in the next cycle.
 * 
 * @param core The CPU core to run the task on.
 * @param callback The function to execute once.
 * @return Task* Pointer to the created task.
 */
Task * onOnce(Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::ONCE, Mode::Active, core, callback);
    task->setCertainly(true);
    tasks[core].push_back(task);
    return task;
}

Task * onOnce(std::function<void(Task *)> callback) {
    auto task = new Task(Type::ONCE, Mode::Active, CURRENT_CORE, callback);
    task->setCertainly(true);
    tasks[CURRENT_CORE].push_back(task);
    return task;
}

/**
 * @brief Add an existing task to execute exactly once.
 * 
 * Adds an already created task to the task list for one-time execution.
 * The task will be executed in the next cycle and then deleted.
 * 
 * @param core The CPU core to run the task on (unused in this overload).
 * @param task Pointer to the existing task to add.
 * @return Task* Pointer to the added task.
 */
Task * onOnce(Core core, Task * task) {
    task->setCertainly(true);
    tasks[core].push_back(task);
    return task;
}


/**
 * @brief Create a tick task that executes every cycle.
 * 
 * Creates a task that will execute on every iteration of the executor's main loop.
 * Tick tasks have the highest priority and run continuously.
 * 
 * @param core The CPU core to run the task on.
 * @param callback The function to execute on each tick.
 * @return Task* Pointer to the created task.
 */
Task * onTick(Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::TICK, Mode::Active, core, callback);
    task->setNext(0);
    tasks[core].push_back(task);
    return task;
}

/**
 * @brief Create a tick task that executes every cycle on the current core.
 * 
 * Creates a task that will execute on every iteration of the executor's main loop
 * on the current CPU core. Tick tasks have the highest priority and run continuously.
 * 
 * @param callback The function to execute on each tick.
 * @return Task* Pointer to the created task.
 */
Task * onTick(std::function<void(Task *)> callback) {
    return onTick(CURRENT_CORE, callback);
}

void mainLoop(void * parameter) {
    int core = (int) parameter;
    std::vector<Task *> toExecute;

    ESP_LOGV(TAG_EXECUTOR, "mainLoop from core %d curr time: %llu", core, minSleepTimeLight[core]);

    while(true) {
        uint64_t startCycleTime = esp_timer_get_time();
        uint64_t startCycleTimeWithRtc = rtcBoot + startCycleTime;

        toExecute.clear();

        lightTasksExists[core] = false;
        tickTasksExists[core] = false;
        minSleepTimeLight[core] = UINT64_MAX;
        minSleepTimeDeep[core] = UINT64_MAX;

        for(int i = 0; i < tasks[core].size(); i++) {
            if(tasks[core][i]->isCertainly()) {
                toExecute.push_back(tasks[core][i]);
                tasks[core][i]->setCertainly(false);

                if(tasks[core][i]->getType() == Type::ONCE) {
                    delete tasks[core][i]; // 1. Освобождаем память
                }

                tasks[core].erase(tasks[core].begin() + i);
                i--;
            }
            else if(tasks[core][i]->getType() == Type::TICK) {
                if(tasks[core][i]->getNext() != UINT64_MAX) {
                    tickTasksExists[core] = true;
                    toExecute.push_back(tasks[core][i]);
                }
                else {
                    delete tasks[core][i]; // 1. Освобождаем память
                    tasks[core].erase(tasks[core].begin() + i);
                    i--;
                }
            }
            else if(tasks[core][i]->getMode() == Mode::Light) {
                if(tasks[core][i]->getNext() != UINT64_MAX) {
                    lightTasksExists[core] = true;

                    // проверяем, не пора ли задаче запускаться
                    if(startCycleTime >= tasks[core][i]->getNext()) {
                        toExecute.push_back(tasks[core][i]);
                        
                        // если это повторяющаяся задача, то обновим ей время отчёта
                        if(tasks[core][i]->getType() == REPEAT) {
                            tasks[core][i]->setNext(tasks[core][i]->getNext() + tasks[core][i]->getInterval());
                        }
                        // если это отложенная задача, то остановим её выполнение
                        else {
                            tasks[core][i]->setNext(UINT64_MAX);
                        }
                    }
                    else if(!tickTasksExists[core]) {
                        minSleepTimeLight[core] = minSleepTimeLight[core] < tasks[core][i]->getNext() ? minSleepTimeLight[core] : tasks[core][i]->getNext();
                    }
                }
                else {
                    delete tasks[core][i]; // 1. Освобождаем память
                    tasks[core].erase(tasks[core].begin() + i);
                    i--;
                }
            }
            else if(tasks[core][i]->getMode() == Mode::Deep) {
                if(deepTasksTimeFast[i] != UINT64_MAX) {
                    if(startCycleTimeWithRtc >= deepTasksTimeFast[i]) {
                        toExecute.push_back(tasks[core][i]);
                        
                        // если это повторяющаяся задача, то обновим ей время отчёта
                        if(tasks[core][i]->getType() == REPEAT) {
                            deepTasksTimeFast[i] += tasks[core][i]->getInterval();
                        }
                        // если это отложенная задача, то остановим её выполнение
                        else {
                            deepTasksTimeFast[i] = UINT64_MAX;
                        }
                    }
                    else if(!tickTasksExists[core] && !lightTasksExists[core]) {
                        minSleepTimeDeep[core] = minSleepTimeDeep[core] < deepTasksTimeFast[i] ? minSleepTimeDeep[core] : deepTasksTimeFast[i];
                    }
                }
            }
        }

        // if(core == 1) {
        //     ESP_LOGV(TAG_EXECUTOR, "tasks %d, core %d, core ready %d, %d, light sleep %d, %d: %d", 
        //         tasks[core].size(), 
        //         core, coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1], BOOL_OR(lightTasksExists, SOC_CPU_CORES_NUM));
        // }

        const bool tickTasksExistsFinal = BOOL_OR(tickTasksExists, SOC_CPU_CORES_NUM);
        const bool lightTasksExistsFinal = BOOL_OR(lightTasksExists, SOC_CPU_CORES_NUM);
        const bool coreSleepReadyFinal = BOOL_AND(coreSleepReady, SOC_CPU_CORES_NUM);

        if(toExecute.size() > 0) {
            coreSleepReady[core] = false;

            for(Task * task : toExecute) {
                task->execute();
            }
        }
        else if(tickTasksExistsFinal || activeTasksCount > 0 || interruptLevel == Mode::Active) {
            //
        }
        else if(!goToSleep && (lightTasksExistsFinal || interruptLevel == Mode::Light)) {
            if(coreSleepReadyFinal) {
                goToSleep = true;

                if(MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) != UINT64_MAX) {
                    esp_sleep_enable_timer_wakeup(MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM) - esp_timer_get_time());
                    timerIsRunning = true;
                }
                else if(timerIsRunning) {
                    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                    timerIsRunning = false;
                }
                
                if(interruptLevel == Mode::Light) {
                    esp_sleep_enable_gpio_wakeup();
                }

                ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, light sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                ESP_LOGV(TAG_EXECUTOR, "esp_light_sleep_start from core %d curr time: %llu all time: %llu", core, minSleepTimeLight[core], MIN_IN_ARRAY(minSleepTimeLight, SOC_CPU_CORES_NUM));
                fflush(stdout);
                vTaskDelay(pdMS_TO_TICKS(100));
                esp_light_sleep_start();

                coreSleepReady[core] = false;
                goToSleep = false;
            }
            else {
                coreSleepReady[core] = true;
            }
        }
        else if(!goToSleep) {
            if(coreSleepReadyFinal) {
                goToSleep = true;

                    for(int i=0; i < DEEP_TASKS_STACK; i++) {
                        deepTasksTime[i] = deepTasksTimeFast[i];
                    }

                    if(MIN_IN_ARRAY(minSleepTimeDeep, SOC_CPU_CORES_NUM) != UINT64_MAX) {
                        esp_sleep_enable_timer_wakeup(MIN_IN_ARRAY(minSleepTimeDeep, SOC_CPU_CORES_NUM) - rts_us());
                        timerIsRunning = true;
                    }
                    else if(timerIsRunning) {
                        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                        timerIsRunning = false;
                    }
            
                    ESP_LOGV(TAG_EXECUTOR, "tasks %d, core %d, core ready %d, %d: %d, light sleep %d, %d: %d", 
                        tasks[core].size(), 
                        core, coreSleepReady[0], coreSleepReady[1], coreSleepReadyFinal, lightTasksExists[0], lightTasksExists[1], lightTasksExistsFinal);

                    //ESP_LOGV(TAG_EXECUTOR, "core ready %d, %d, light sleep %d, %d", coreSleepReady[0], coreSleepReady[1], lightTasksExists[0], lightTasksExists[1]);
                    ESP_LOGV(TAG_EXECUTOR, "esp_deep_sleep_start");
                    fflush(stdout);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    esp_deep_sleep_start();
            }
            else {
                coreSleepReady[core] = true;
            }
        }

        vTaskDelay(1); // yield to other tasks
    }
}

/**
 * @brief Start the async executor main loop.
 * 
 * This function starts the main execution loop that processes all scheduled tasks.
 * It runs indefinitely, checking for tasks to execute, managing sleep modes,
 * and handling task scheduling. This function should be called once after
 * setting up all tasks.
 * 
 * The executor will:
 * - Process tick tasks on every iteration
 * - Execute delayed and repeating tasks when their time comes
 * - Enter light sleep when only light tasks are pending
 * - Enter deep sleep when only deep tasks are pending
 * - Handle active tasks using ESP timers
 * 
 * @note This function never returns - it runs the main execution loop indefinitely.
 * @throws esp_system_abort if the executor has already been started.
 */
void start() {
    if(startFlag) {
        esp_system_abort("Already started");
    }

    rtcBoot = rts_us();

    for(int i=0; i < DEEP_TASKS_STACK; i++) {
        deepTasksTimeFast[i] = deepTasksTime[i];
    }

    startFlag = true;

    for(int core = 0; core < SOC_CPU_CORES_NUM; core++) {
        xTaskCreatePinnedToCore(mainLoop, // Task function.
                        "",     // name of task. //
                        10000,       // Stack size of task //
                        (void *) core,        // parameter of the task //
                        1,           // priority of the task //
                        &taskLoopCore0,      // Task handle to keep track of created task //
                        core);          // pin task to core 0 // 
    }
}

}