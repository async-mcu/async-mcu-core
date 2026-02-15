#pragma once

#include <functional>
#include <async/Definitions.h>
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

    public: 
        Task(Type type, SleepMode sleepMode, Core core, Duration * delay, Duration * interval, std::function<void(Task &)> callback);
            
        Task(Type type, SleepMode sleepMode, Core core, Duration * delay, std::function<void(Task &)> callback);

        Task(Type type, SleepMode sleepMode, Core core, std::function<void(Task &)> callback);

        ~Task();

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

        void setValue(void * value);

        void cancel();
};

}