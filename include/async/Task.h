#pragma once

#include <functional>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <async/Globals.h>
#include <async/Duration.h>

namespace async {

class Task {
    private: 
        SleepMode sleepMode;
        Type type;
        Core core;
        uint64_t next = UINT64_MAX;
        bool certainly = false;
        std::function<void(Task &)> callback;
        Duration * delay = nullptr;
        Duration * interval = nullptr;
        esp_timer_handle_t * timer = nullptr;
        void * value = nullptr;
        bool counted = false;     // учтена ли в activeTasksCount
        bool cancelled = false;   // задача отменена (cancel)

    public: 
        Task(Type type, SleepMode sleepMode, Core core, Duration * delay, Duration * interval, std::function<void(Task &)> callback);
            
        Task(Type type, SleepMode sleepMode, Core core, Duration * delay, std::function<void(Task &)> callback);

        Task(Type type, SleepMode sleepMode, Core core, std::function<void(Task &)> callback);

        ~Task();

        Task(const Task &) = delete;
        Task & operator=(const Task &) = delete;

        Type getType();

        Duration & getInterval();

        Duration & getDelay();

        SleepMode getSleepMode();

        esp_timer_handle_t & getTimer();

        void setTimer(esp_timer_handle_t * timer);

        uint64_t getNext();

        void setNext(uint64_t value);

        void execute();

        void schedule();

        void setCertainly(bool value);

        bool isCertainly();

        void * getValue();

        void IRAM_ATTR setValue(void * value);

        bool isCancelled();
        bool isCounted();
        void setCounted(bool value);

        void cancel();
};

    // ===== API планирования задач (определения в Task.cpp) =====
    // rts_us/rts_ms и setSleepMode/getSleepMode — inline в <async/Globals.h>.

    std::vector<Task *> getCoreTasks(Core core);
    TaskHandle_t getCoreTaskHandler(Core core);

    // checkDeepSleepTaskCanBeAdded — внутренняя (static в Task.cpp).

    void beforeEnterSleep(std::function<void(SleepMode)> callback);
    void afterWakeUp(std::function<void(SleepMode)> callback);

    void callTaskExecute(void *arg);
    void notifyActiveTaskCancelled(Task & task);

    Task * onDelay(SleepMode sleepMode, Duration * delay, Core core, std::function<void(Task &)> callback);
    template<SleepMode sleepMode>
    Task * onDelay(Duration * delay, Core core, std::function<void(Task &)> callback) {
        return onDelay(sleepMode, delay, core, callback);
    }
    template<SleepMode sleepMode>
    Task * onDelay(Duration * delay, std::function<void(Task &)> callback) {
        return onDelay<sleepMode>(delay, CURRENT_CORE, callback);
    }

    Task * onRepeat(SleepMode sleepMode, Duration * interval, Duration * startDelay, Core core, std::function<void(Task &)> callback);
    template<SleepMode sleepMode>
    Task * onRepeat(Duration * interval, Duration * startDelay, Core core, std::function<void(Task &)> callback) {
        return onRepeat(sleepMode, interval, startDelay, core, callback);
    }
    template<SleepMode sleepMode>
    Task * onRepeat(Duration * interval, Duration * startDelay, std::function<void(Task &)> callback) {
        return onRepeat(sleepMode, interval, startDelay, CURRENT_CORE, callback);
    }
    template<SleepMode mode>
    Task * onRepeat(Duration * interval, Core core, std::function<void(Task &)> callback) {
        return onRepeat<mode>(interval, interval, core, callback);
    }
    template<SleepMode sleepMode>
    Task * onRepeat(Duration * interval, std::function<void(Task &)> callback) {
        return onRepeat<sleepMode>(interval, interval, CURRENT_CORE, callback);
    }

    Task * onDemand(Core core, std::function<void(Task &)> callback);
    Task * onDemand(std::function<void(Task &)> callback);

    Task * onOnce(Core core, std::function<void(Task &)> callback);
    Task * onOnce(std::function<void(Task &)> callback);
    Task * onOnce(Core core, Task * task);

    Task * onInit(Core core, std::function<void(Task &)> callback);

    Task * onTick(Core core, std::function<void(Task &)> callback);
    Task * onTick(std::function<void(Task &)> callback);

}