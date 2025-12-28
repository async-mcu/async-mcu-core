#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include "esp_sleep.h"
#include "driver/gptimer.h"
#include "esp32-hal.h"
#include "soc/rtc.h"
#include "esp_system.h"
#include "esp_private/panic_internal.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_event.h"
#include <async/Duration.h>
#include <async/Task.h>
#include <async/Mode.h>
#include "esp_wifi.h"

#define CURRENT_CORE (esp_cpu_get_core_id() == 0 ? CORE0 : CORE1)
#define DEEP_TASKS_STACK 50

namespace async {
    
int64_t rts_us() { // slow
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    return time_us;
}

int64_t rts_ms() {
    return rts_us()/1000ULL;
}

std::vector<Task *> tasks;

RTC_DATA_ATTR uint64_t deepTasksTime[DEEP_TASKS_STACK]; // agata call dad
              uint64_t deepTasksTimeFast[DEEP_TASKS_STACK]; // agata call dad
bool startFlag = false;
bool timerIsRunning = false;
int activeTasksCount = 0;

void checkDeepSleepTaskCanBeAdded() {
    if(startFlag) {
        esp_system_abort("Deep tasks can only be added before the start() method.");
    }
}

static void callTaskExecute(void *arg) {
    Task *obj = (Task *)arg;
    obj->execute();

    if(obj->getMode() == Mode::Active && obj->getType() == Type::DELAY) {
        activeTasksCount--;
    }
}

/**
 * 
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
            deepTasksTime[tasks.size()] = task->getDelay();
        }

        tasks.push_back(task);
    }

    return task;
}

template<Mode mode>
Task * onDelay(Duration * delay, std::function<void(Task *)> callback) {
    return onDelay<mode>(delay, CURRENT_CORE, callback);
}

/**
 * 
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
            deepTasksTime[tasks.size()] = task->getDelay();
        }

        tasks.push_back(task);
    }

    return task;
}

template<Mode mode> 
Task * onRepeat(Duration * interval, Core core, std::function<void(Task *)> callback) {
    return onRepeat<mode>(interval, interval, core, callback);
}

template<Mode mode> 
Task * onRepeat(Duration * interval, std::function<void(Task *)> callback) {
    return onRepeat<mode>(interval, interval, CURRENT_CORE, callback);
}

/**
 * 
 */
Task * onDemand(Core core, std::function<void(Task *)> callback) {
    return new Task(Type::DEMAND, Mode::Active, core, callback);
}

/**
 * 
 */
Task * onOnce(Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::ONCE, Mode::Active, core, callback);
    tasks.push_back(task);
    return task;
}

/**
 * 
 */
Task * onOnce(Core core, Task * task) {
    tasks.push_back(task);
    return task;
}

/**
 * 
 */
Task * onTick(Core core, std::function<void(Task *)> callback) {
    auto task = new Task(Type::TICK, Mode::Active, core, callback);
    tasks.push_back(task);
    return task;
}

Task * onTick(std::function<void(Task *)> callback) {
    return onTick(CURRENT_CORE, callback);
}

void start() {
    if(startFlag) {
        esp_system_abort("Already started");
    }

    uint64_t rtcBoot = rts_us();

    for(int i=0; i < DEEP_TASKS_STACK; i++) {
        deepTasksTimeFast[i] = deepTasksTime[i];
    }

    startFlag = true;
    std::vector<Task *> toExecute;

    while(true) {
        uint64_t minSleepTimeDeep = UINT64_MAX;
        uint64_t minSleepTimeLight = UINT64_MAX;

        bool tickTasksExists = false;
        bool lightTasksExists = false;
        uint64_t startCycleTime = esp_timer_get_time();
        uint64_t startCycleTimeWithRtc = rtcBoot + startCycleTime;

        toExecute.clear();

        for(int i = 0; i < tasks.size(); i++) {
            if(tasks[i]->isCertainly()) {
                toExecute.push_back(tasks[i]);
                tasks[i]->setCertainly(false);
                
                if(tasks[i]->getType() == Type::ONCE) {
                    delete tasks[i]; // 1. Освобождаем память
                    tasks.erase(tasks.begin() + i);
                    i--;
                }
            }
            else if(tasks[i]->getType() == Type::TICK && tasks[i]->getNext() != UINT64_MAX) {
                tickTasksExists = true;
                toExecute.push_back(tasks[i]);
            }
            else if(tasks[i]->getMode() == Mode::Light) {
                if(tasks[i]->getNext() != UINT64_MAX) {
                    lightTasksExists = true;

                    // проверяем, не пора ли задаче запускаться
                    if(startCycleTime >= tasks[i]->getNext()) {
                        toExecute.push_back(tasks[i]);
                        
                        // если это повторяющаяся задача, то обновим ей время отчёта
                        if(tasks[i]->getType() == REPEAT) {
                            tasks[i]->setNext(tasks[i]->getNext() + tasks[i]->getInterval());
                        }
                        // если это отложенная задача, то остановим её выполнение
                        else {
                            tasks[i]->setNext(UINT64_MAX);
                        }
                    }
                    else if(!tickTasksExists) {
                        minSleepTimeLight = min(minSleepTimeLight, tasks[i]->getNext());
                    }
                }
            }
            else if(tasks[i]->getMode() == Mode::Deep) {
                if(deepTasksTimeFast[i] != UINT64_MAX) {
                    if(startCycleTimeWithRtc >= deepTasksTimeFast[i]) {
                        toExecute.push_back(tasks[i]);
                        
                        // если это повторяющаяся задача, то обновим ей время отчёта
                        if(tasks[i]->getType() == REPEAT) {
                            deepTasksTimeFast[i] += tasks[i]->getInterval();
                        }
                        // если это отложенная задача, то остановим её выполнение
                        else {
                            deepTasksTimeFast[i] = UINT64_MAX;
                        }
                    }
                    else if(!tickTasksExists && !lightTasksExists) {
                        minSleepTimeDeep = min(minSleepTimeDeep, deepTasksTimeFast[i]);
                    }
                }
            }
        }

        if(toExecute.size() > 0) {
            for(Task * task : toExecute) {
                task->execute();
            }
        }
        else if(tickTasksExists || activeTasksCount > 0) {
            //
        }
        else if(lightTasksExists) {
            if(minSleepTimeLight != UINT64_MAX) {
                esp_sleep_enable_timer_wakeup(minSleepTimeLight);
                timerIsRunning = true;
            }
            else if(timerIsRunning) {
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                timerIsRunning = false;
            }

            esp_light_sleep_start();
        }
        else {
            for(int i=0; i < DEEP_TASKS_STACK; i++) {
                deepTasksTime[i] = deepTasksTimeFast[i];
            }

            if(minSleepTimeDeep != UINT64_MAX) {
                esp_sleep_enable_timer_wakeup(minSleepTimeDeep - rts_us());
                timerIsRunning = true;
            }
            else if(timerIsRunning) {
                esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
                timerIsRunning = false;
            }
    
            esp_deep_sleep_start();
        }

        //vTaskDelay(1);
    }
}

}