#pragma once

#include <functional>
#include <async/Mode.h>
#include <async/Duration.h>
#include <async/Executor.h>

namespace async {

enum Type {
  REPEAT = 0,
  DELAY = 10,
  DEMAND = 20,
  TICK = 30,
  ONCE = 40,
  INTERR = 50
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
        uint64_t next = UINT64_MAX;
        bool certainly = false;
        std::function<void(Task *)> callback;
        
        Duration * delay = nullptr;
        Duration * interval = nullptr;
        esp_timer_handle_t * timer = nullptr;
        void * value = nullptr;

    public: 
        Task(Type type, Mode mode, Core core, Duration * delay, Duration * interval, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), delay(delay), interval(interval), callback(callback) {}
            
        Task(Type type, Mode mode,Core core, Duration * delay, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), delay(delay), callback(callback) {}

        Task(Type type, Mode mode, Core core, std::function<void(Task *)> callback)
            : type(type), mode(mode), core(core), callback(callback) {}

        ~Task() {
            ets_printf("remove task mode %d, type %d!\n", mode, type);
            if(delay != nullptr) {
                delete delay;
                delay = nullptr;
            }   
            if(interval != nullptr) {
                delete interval;
                interval = nullptr;
            }
            if(timer != nullptr) {
                esp_timer_delete(*timer);
                delete timer;
                timer = nullptr;
            }
            if(value != nullptr) {
                delete value;
                value = nullptr;
            }
        }

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
                delete timer;
            }

            next = UINT64_MAX;
        }
};

}