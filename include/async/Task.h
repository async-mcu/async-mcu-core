#pragma once

#include <functional>
#include <async/Mode.h>
#include <async/Duration.h>
#include <async/Executor.h>

namespace async {

enum Type {
  REPEAT = 0,
  DELAY = 1,
  DEMAND = 2,
  TICK = 3,
  ONCE = 4
};

enum Core {
    CORE0 = 0,
    CORE1 = 1
};

class Task;
Task * onOnce(Core core, Task * task) ;

class Task {
    private: 
        Mode mode;
        Type type;
        Core core;
        Duration * delay = nullptr;
        Duration * interval = nullptr;
        esp_timer_handle_t * timer = NULL;
        uint64_t next = UINT64_MAX;
        std::function<void(Task *)> callback;
        void * value;
        bool certainly = false;

    public: 
        Task(Type type, Mode mode, Core core, Duration * delay, Duration * interval, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), delay(delay), interval(interval), callback(callback) {}
            
        Task(Type type, Mode mode,Core core, Duration * delay, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), delay(delay), callback(callback) {}

        Task(Type type, Mode mode, Core core, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), callback(callback) {}

        Type getType() {
            return type;
        }

        Duration & getInterval() {
            return * interval;
        }

        Duration & getDelay() {
            return * delay;
        }

        Mode getMode() {
            return mode;
        }

        esp_timer_handle_t & getTimer() {
            return * timer;
        }

        void setTimer(esp_timer_handle_t * timer) {
            this->timer = timer;
        }

        uint64_t getNext() {
            return next;
        }

        void setNext(uint64_t value) {
            this->next = value;
        }

        void execute() {
            callback(this);
        }

        void schedule() {
            certainly = true;
            onOnce(core, this);
        }

        void setCertainly(bool value) {
            this->certainly = value;
        }

        bool isCertainly() {
            return certainly;
        }

        void * getValue() {
            return value;
        }

        void setValue(void * value) {
            this->value = value;
        }

        void cancel() {
            if(timer != NULL) {
                esp_timer_delete(*timer);
            }

            next = UINT64_MAX;
        }
};

}