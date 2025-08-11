#pragma once
#include <Arduino.h>
#include <async/Callbacks.h>
#include <async/Tick.h>
#include <async/Task.h>
#include <vector>

/**
 * @brief Interrupt Service Routine for Pin interrupts.
 * @param arg Pointer to argument, typically a Task object.
 */
IRAM_ATTR void ISR(void* arg);

namespace async {
    /**
     * @brief Asynchronous Pin class for handling digital/analog IO and interrupts.
     *
     * This class wraps an Arduino pin and provides asynchronous interrupt handling
     * and task management for rising and falling edges.
     */
    class Pin : public Tick {
        private:
            int pin; ///< Pin number or interrupt number.
            int mode; ///< Pin mode (INPUT, OUTPUT, etc).
            int value; ///< Last written value.
            Task * interruptTask; ///< Task triggered by pin interrupt.
            std::vector<async::Task*> handlersRising; ///< Tasks for rising edge.
            std::vector<async::Task*> handlersFalling; ///< Tasks for falling edge.
        public:

        /**
         * @brief Construct a new Pin object.
         * @param pin Arduino pin number.
         * @param mode Pin mode (default INPUT_PULLUP).
         * @param val Initial value (default HIGH).
         */
        Pin(int pin, int mode = INPUT_PULLUP, int val = HIGH): pin(digitalPinToInterrupt(pin)), mode(mode), value(val) {
            interruptTask = new Task(Task::DEMAND, [this]() {
                if(digitalRead() == HIGH) {
                    for(int i=0; i < this->handlersRising.size(); i++) {
                        handlersRising.at(i)->demand();
                    }
                }
                else {
                    for(int i=0; i < this->handlersFalling.size(); i++) {
                        handlersFalling.at(i)->demand();
                    }
                }
            });
        };

        /**
         * @brief Start the pin, setting its mode and initial value.
         * @return true if successful.
         */
        bool start() override {
            setMode(mode);

            if(mode == OUTPUT) {
                digitalWrite(value);
            }

            return true;
        }

        /**
         * @brief Write a digital value to the pin.
         * @param state HIGH or LOW.
         */
        void digitalWrite(bool state) {
            if(mode != OUTPUT) {
                setMode(OUTPUT);
            }

            ::digitalWrite(pin, state);
        }
        
        /**
         * @brief Write an analog value to the pin.
         * @param state Analog value.
         */
        void analogWrite(int state) {
            if(mode != OUTPUT) {
                setMode(OUTPUT);
            }

            ::analogWrite(pin, state);
        }

        /**
         * @brief Read the digital value from the pin.
         * @return int HIGH or LOW.
         */
        int digitalRead() {
            return ::digitalRead(pin);
        }

        /**
         * @brief Read the analog value from the pin.
         * @return int Analog value (0-1023).
         */
        int analogRead() {
            return ::analogRead(pin);
        }

        /**
         * @brief Get the current mode of the pin.
         * @return int Pin mode (INPUT, OUTPUT, etc).
         */
        int getMode() {
            return mode;
        }

        /**
         * @brief Set the mode of the pin and configure interrupt handling.
         * @param mode Pin mode (INPUT, OUTPUT, etc).
         */
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

        /**
         * @brief Get the pin number.
         * @return int Pin number.
         */
        int getPin() {
            return pin;
        }
        
        /**
         * @brief Get the last written value to the pin.
         * @return int Last value written.
         */
        void addTask(Task * task) {
            this->handlersFalling.push_back(task);
        }

        /**
         * @brief Register a callback for pin interrupts.
         * @param edge RISING or FALLING.
         * @param callback Function to call.
         */
        void onInterrupt(int edge, VoidCallback callback) {
            auto task = new Task(Task::DEMAND, callback);

            if(edge == RISING) {
                this->handlersRising.push_back(task);
            }
            else {
                this->handlersFalling.push_back(task);
            }

        }

        /**
         * @brief Remove a registered interrupt task.
         * @param task Pointer to Task.
         */ 
        void removeInterrupt(Task * task) {
            this->handlersRising.erase(std::remove(this->handlersRising.begin(), this->handlersRising.end(), task));
            this->handlersFalling.erase(std::remove(this->handlersRising.begin(), this->handlersRising.end(), task));
        }

        /**
         * @brief Tick handler for the pin and its tasks.
         * @return true if successful.
         */
        bool tick() {
            interruptTask->tick();

            for(int i=0; i < this->handlersRising.size(); i++) {
                handlersRising.at(i)->tick();
            }
            for(int i=0; i < this->handlersFalling.size(); i++) {
                handlersFalling.at(i)->tick();
            }

            return true;
        };
    };
}