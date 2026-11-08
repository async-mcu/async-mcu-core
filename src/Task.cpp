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

    // ===== Финализация Active-задач (#7/#32) =====
    // Мьютекс сериализует «claim -> декремент -> остановка таймера -> удаление»
    // между диспетчем esp_timer (callTaskExecute) и произвольным cancel().
    // Инвариант: объект Active-задачи удаляется ТОЛЬКО в контексте диспатча,
    // под этим мьютексом; проигравший claim объект не трогает вовсе.
    // Создаётся лениво при первой Active-регистрации (контекст пользователя;
    // до старта воркеров выполнение однопоточное).
    static SemaphoreHandle_t activeFinalizeMutex = NULL;

    static void ensureActiveFinalizeLock() {
        if (activeFinalizeMutex == NULL) {
            activeFinalizeMutex = xSemaphoreCreateMutex();
        }
    }

Task::~Task() {
    ESP_LOGV(TAG_TASK, "Remove task sleep mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
    // Сначала таймер: пока он жив, колбэк esp_timer может читать delay/interval —
    // освобождать члены можно только после esp_timer_delete.
    if(timer != nullptr) {
        esp_timer_delete(*timer);
        delete timer;
        timer = nullptr;
    }
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

bool Task::tryClaimFinale() {
    return counted.exchange(false, std::memory_order_acq_rel);
}

void Task::cancel() {
    // Active-задачи (DELAY/REPEAT): завершение через claim под мьютексом
    // финализации — ровно один декремент (#7), остановка вооружённого таймера
    // (#32: delete без stop не останавливает armed esp_timer), и никакого
    // обращения к объекту после проигранного claim (диспетч мог уже удалить).
    // Объект здесь НЕ удаляем: идущий диспатч удалит после execute; если диспатча
    // не будет — residual-утечка (класс #23, задокументирована).
    if (sleepMode == SleepMode::Active && (type == Type::DELAY || type == Type::REPEAT)) {
        ensureActiveFinalizeLock();
        xSemaphoreTake(activeFinalizeMutex, portMAX_DELAY);
        if (tryClaimFinale()) {
            activeTasksCount--;
            if (timer != nullptr) {
                esp_timer_stop(*timer);
                esp_timer_delete(*timer);
                delete timer;
                timer = nullptr;
            }
        }
        xSemaphoreGive(activeFinalizeMutex);
        return;
    }

    // Не-Active (Light/Deep/None): пометить — скан воркера не исполнит.
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

    // Регистрация задачи на ядро. До старта — прямой push_back (worker'ов ещё нет);
    // после старта — через очередь coreIncomingQueue + notify, чтобы push_back в coreTasks
    // шёл только из worker-потока ядра-адресата (исключает гонку с кросс-контекстной
    // регистрацией из ISR-dispatcher / соседнего ядра / колбэков). См. EXECUTOR.md §6.3, §6.9.
    static void registerTask(Core core, Task * task) {
        if (!isStarted() || coreIncomingQueue[core] == NULL) {
            coreTasks[core].push_back(task);
            return;
        }
        // #22: self-регистрация — текущий контекст и есть worker ядра-адресата
        // (колбэк исполняется им). Единственный потребитель очереди заблокирован
        // в этом колбэке: заполненная очередь + portMAX_DELAY = вечный дедлок.
        // Мутируем вектор напрямую — по построению worker-модели (#2) только
        // этот поток трогает coreTasks своего ядра, а итерации скана в момент
        // исполнения колбэков нет (скан завершается до toExecute).
        if (xTaskGetCurrentTaskHandle() == coreTaskHandlers[core]) {
            coreTasks[core].push_back(task);
            return;
        }
        xQueueSend(coreIncomingQueue[core], &task, portMAX_DELAY);
        xTaskNotifyGive(coreTaskHandlers[core]);
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

        // Одноразовая Active-задача отстрелялась: единая точка финализации
        // (#7/#9/#32). Claim даёт ровно один декремент (конкурентный cancel()
        // под тем же мьютексом либо сделал свой, либо получит claim=false и
        // не тронет объект). Удаление — всегда здесь, в контексте диспатча.
        // Контракт: указатель onDelay<Active> невалиден после срабатывания.
        if (task->getSleepMode() == SleepMode::Active && task->getType() == Type::DELAY) {
            ensureActiveFinalizeLock();
            xSemaphoreTake(activeFinalizeMutex, portMAX_DELAY);
            if (task->tryClaimFinale()) {
                activeTasksCount--;
            }
            delete task;
            xSemaphoreGive(activeFinalizeMutex);
            return;
        }

        // Периодическая Active: если её забрал cancel (counted уже false) —
        // этот выстрел был последним, объект освобождаем здесь же (#32).
        if (task->getSleepMode() == SleepMode::Active && task->getType() == Type::REPEAT) {
            if (!task->isCounted()) {
                ensureActiveFinalizeLock();
                xSemaphoreTake(activeFinalizeMutex, portMAX_DELAY);
                if (!task->isCounted()) {   // перепроверка под локом
                    delete task;
                }
                xSemaphoreGive(activeFinalizeMutex);
            }
            return;
        }
    }

    Task * onDelay(SleepMode sleepMode, Duration *delay, Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::DELAY, sleepMode, core, delay, callback);

        if (sleepMode == SleepMode::Active) {
            ensureActiveFinalizeLock();   // мьютекс финализации до возможного выстрела (#7/#32)
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

            registerTask(core, task);
        }

        return task;
    }

    Task * onRepeat(SleepMode sleepMode, Duration *interval, Duration *startDelay, Core core,
                    std::function<void(Task &)> callback) {
        auto task = new Task(Type::REPEAT, sleepMode, core, startDelay, interval, callback);

        if (sleepMode == SleepMode::Active) {
            ensureActiveFinalizeLock();   // мьютекс финализации до возможного выстрела (#7/#32)
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

            registerTask(core, task);
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
        registerTask(core, task);
        return task;
    }

    Task * onOnce(std::function<void(Task &)> callback) {
        return onOnce(CURRENT_CORE, callback);
    }

    Task * onOnce(Core core, Task * task) {
        task->setCertainly(true);
        registerTask(core, task);
        return task;
    }

    Task * onInit(Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::INIT, SleepMode::Active, core, callback);
        task->setCertainly(true);
        registerTask(core, task);
        return task;
    }

    Task * onTick(Core core, std::function<void(Task &)> callback) {
        auto task = new Task(Type::TICK, SleepMode::Active, core, callback);
        task->setNext(0);
        registerTask(core, task);
        return task;
    }

    Task * onTick(std::function<void(Task &)> callback) {
        return onTick(CURRENT_CORE, callback);
    }

}
