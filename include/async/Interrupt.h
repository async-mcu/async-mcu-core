#pragma once
#include <Arduino.h>
#include <async/Tick.h>
#include <async/Task.h>
#include <async/LinkedList.h>

#ifndef ARDUINO_ARCH_ESP32
void ISR(); {
    if (bitRead(EIFR, INTF0) == HIGH) {
        interruptPin = 0;
    }
    if (bitRead(EIFR, INTF1) == HIGH) {
        interruptPin = 1;
    }

    Interrupt::task->demand();
}
#else
IRAM_ATTR void ISR(void* arg);
#endif

namespace async {
    class Interrupt : public Tick {
        private:
            int pin;
            int mode;
            bool init;
            Task * task;
            LinkedList<async::Task*> handlers;
        public:

        Interrupt(int pin, int mode): pin(digitalPinToInterrupt(pin)), mode(mode) {
            if(mode == RISING) {
                pinMode(pin, INPUT);
            }
            else if(mode == FALLING) {
                pinMode(pin, INPUT_PULLUP);
            }

            task = new Task(Task::DEMAND, [this]() {
                for(int i=0; i < this->handlers.size(); i++) {
                    this->handlers.get(i)->demand();
                }
            });

            #ifndef ARDUINO_ARCH_ESP32

            if(handlers[val].size() == 0) {
                Serial.println("attachInterrupt");
                attachInterrupt(val, val == 0 ? ISR0 : ISR1, mode);
            }

            handlers[val].append(this);
            #else
                Serial.println(pin);
            #endif
        };

        int getValue() {
            return digitalRead(pin);
        }

        int getMode() {
            return mode;
        }

        int getPin() {
            return pin;
        }
        
        void addTask(Task * task) {
            this->handlers.append(task);
        }

        Task * onInterrupt(voidCallback callback) {
            auto task = new Task(Task::DEMAND, callback);
            this->handlers.append(task);
            return task;
        }

        bool tick() {
            if(!init) {
                attachInterruptArg(pin, ISR, task, mode);
                init = true;
            }

            return task->tick();
        };
    };
}