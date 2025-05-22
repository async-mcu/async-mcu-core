#pragma once
#include <Arduino.h>
#include <async/Callbacks.h>
#include <async/Tick.h>
#include <async/Task.h>
#include <async/FastList.h>

IRAM_ATTR void ISR(void* arg);

namespace async {
    class Pin : public Tick {
        private:
            int pin;
            int mode;
            int edge;
            Task * task;
            FastList<async::Task*> handlersRising;
            FastList<async::Task*> handlersFalling;
        public:

        Pin(int pin, int mode = INPUT_PULLUP): pin(digitalPinToInterrupt(pin)), mode(mode), edge(edge) {
            task = new Task(Task::DEMAND, [this]() {
                if(digitalRead() == HIGH) {
                    for(int i=0; i < this->handlersRising.size(); i++) {
                        handlersRising.get(i)->demand();
                    }
                }
                else {
                    for(int i=0; i < this->handlersFalling.size(); i++) {
                        handlersFalling.get(i)->demand();
                    }
                }
            });
        };

        bool start() override {
            setMode(mode);
            return true;
        }

        void digitalWrite(bool state) {
            if(mode != OUTPUT) {
                setMode(OUTPUT);
            }

            ::digitalWrite(pin, state);
        }

        int digitalRead() {
            return ::digitalRead(pin);
        }

        int getMode() {
            return mode;
        }

        void setMode(int mode) {
            this->mode = mode;

            if(mode == OUTPUT) {
                detachInterrupt(pin);
                pinMode(pin, mode);
            }
            else {
                pinMode(pin, mode);
                attachInterruptArg(pin, ISR, task, CHANGE);
            }
        }

        void setEdge(int edge) {
            setMode(edge != RISING ? INPUT : INPUT_PULLUP);
        }

        int getPin() {
            return pin;
        }
        
        void addTask(Task * task) {
            this->handlersFalling.append(task);
        }

        Task * onInterrupt(int edge, VoidCallback callback) {
            auto task = new Task(Task::DEMAND, callback);

            if(edge == RISING) {
                this->handlersRising.append(task);
            }
            else {
                this->handlersFalling.append(task);
            }

            return task;
        }

        void removeInterrupt(Task * task) {
            this->handlersRising.remove(task);
            this->handlersFalling.remove(task);
        }

        bool tick() {
            return task->tick();
        };
    };
}