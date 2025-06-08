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
            int value;
            Task * interruptTask;
            FastList<async::Task*> handlersRising;
            FastList<async::Task*> handlersFalling;
        public:

        Pin(int pin, int mode = INPUT_PULLUP, int val = HIGH): pin(digitalPinToInterrupt(pin)), mode(mode), value(val) {
            interruptTask = new Task(Task::DEMAND, [this]() {
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

            if(mode == OUTPUT) {
                digitalWrite(value);
            }

            return true;
        }

        void digitalWrite(bool state) {
            if(mode != OUTPUT) {
                setMode(OUTPUT);
            }

            ::digitalWrite(pin, state);
        }
        void analogWrite(int state) {
            if(mode != OUTPUT) {
                setMode(OUTPUT);
            }

            ::analogWrite(pin, state);
        }

        int digitalRead() {
            return ::digitalRead(pin);
        }

        int analogRead() {
            return ::analogRead(pin);
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
                attachInterruptArg(pin, ISR, interruptTask, CHANGE);
            }
        }

        int getPin() {
            return pin;
        }
        
        void addTask(Task * task) {
            this->handlersFalling.append(task);
        }

        void onInterrupt(int edge, VoidCallback callback) {
            auto task = new Task(Task::DEMAND, callback);

            if(edge == RISING) {
                this->handlersRising.append(task);
            }
            else {
                this->handlersFalling.append(task);
            }

        }

        void removeInterrupt(Task * task) {
            this->handlersRising.remove(task);
            this->handlersFalling.remove(task);
        }

        bool tick() {
            interruptTask->tick();

            for(int i=0; i < this->handlersRising.size(); i++) {
                handlersRising.get(i)->tick();
            }
            for(int i=0; i < this->handlersFalling.size(); i++) {
                handlersFalling.get(i)->tick();
            }

            return true;
        };
    };
}