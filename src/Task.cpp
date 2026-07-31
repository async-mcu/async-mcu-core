#include <async/Task.h>
#include <async/Executor.h>
#include <async/Globals.h>
#include "sys/time.h"
#include "esp_timer.h"
#include "esp_system.h"

namespace async {

Task::Task(Type type, SleepMode sleepMode, Core core, Duration * delay, Duration * interval, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), delay(delay), interval(interval), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::Task(Type type, SleepMode sleepMode, Core core, Duration * delay, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), delay(delay), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::Task(Type type, SleepMode sleepMode, Core core, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::~Task() {
    ESP_LOGV(TAG_TASK, "Remove task sleep mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
    // delay и interval могут указывать на один объект: перегрузки onRepeat(interval,…)
    // передают interval и как startDelay, и как interval (Executor.h). Удалить один раз.
    bool delayIsInterval = (delay != nullptr && interval == delay);
    if(delay != nullptr) {
        delete delay;
        delay = nullptr;
    }
    if(interval != nullptr && !delayIsInterval) {
        delete interval;
        interval = nullptr;
    }
    if(timer != nullptr) {
        esp_timer_delete(*timer);
        delete timer;
        timer = nullptr;
    }
}

Type Task::getType() {
    return type;
}

Duration & Task::getInterval() {
    return * interval;
}

Duration & Task::getDelay() {
    return * delay;
}

SleepMode Task::getSleepMode() {
    return sleepMode;
}

esp_timer_handle_t & Task::getTimer() {
    return * timer;
}

void Task::setTimer(esp_timer_handle_t * timer) {
    this->timer = timer;
}

uint64_t Task::getNext() {
    return next;
}

void Task::setNext(uint64_t value) {
    this->next = value;
}

void Task::execute() {
    callback(*this);
}

void Task::schedule() {
    onOnce(core, this);
}

void Task::setCertainly(bool value) {
    this->certainly = value;
}

bool Task::isCertainly() {
    return certainly;
}

void * Task::getValue() {
    return value;
}

void IRAM_ATTR Task::setValue(void * value) {
    this->value = value;
}

bool Task::isCancelled() {
    return cancelled;
}

bool Task::isCounted() {
    return counted;
}

void Task::setCounted(bool value) {
    this->counted = value;
}

void Task::cancel() {
    notifyActiveTaskCancelled(*this);

    if(timer != NULL) {
        esp_timer_delete(*timer);
        delete timer;
        timer = nullptr;
    }

    cancelled = true;
    next = UINT64_MAX;
}

// ===== API планирования задач =====

    std::vector<Task *> getCoreTasks(Core core) {
        return coreTasks[core];
    }

    TaskHandle_t getCoreTaskHandler(Core core) {
        return coreTaskHandlers[core];
    }

    // Внутренняя: проверяет, что Deep-задача добавляется до старта шедулера.
    static void checkDeepSleepTaskCanBeAdded() {
        if (isStarted()) {
            esp_system_abort("Deep tasks can only be added before the start() method.");
        }
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
                // onDelay (DELAY): relative (rts + delay) — иначе вложенный onDelay, зарегистрированный
                // при rts > delay, срабатывает сразу. onRepeat (REPEAT): абсолютный delay — repeat-логика
                // (+= interval) и restore после deep sleep рассчитаны на абсолютные значения.
                task->setNext(task->getType() == Type::DELAY ? (uint64_t) rts_us() + (uint64_t) task->getDelay()
                                                              : (uint64_t) task->getDelay());
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
                    // onDelay → relative (см. ветку Light); onRepeat → абсолютный delay.
                    deepTasksTimeFast[core][idx] = (task->getType() == Type::DELAY) ? (uint64_t) rts_us() + (uint64_t) task->getDelay()
                                                                                    : (uint64_t) task->getDelay();
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
                // onDelay (DELAY): relative (rts + delay) — иначе вложенный onDelay, зарегистрированный
                // при rts > delay, срабатывает сразу. onRepeat (REPEAT): абсолютный delay — repeat-логика
                // (+= interval) и restore после deep sleep рассчитаны на абсолютные значения.
                task->setNext(task->getType() == Type::DELAY ? (uint64_t) rts_us() + (uint64_t) task->getDelay()
                                                              : (uint64_t) task->getDelay());
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
                    // onDelay → relative (см. ветку Light); onRepeat → абсолютный delay.
                    deepTasksTimeFast[core][idx] = (task->getType() == Type::DELAY) ? (uint64_t) rts_us() + (uint64_t) task->getDelay()
                                                                                    : (uint64_t) task->getDelay();
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

}
