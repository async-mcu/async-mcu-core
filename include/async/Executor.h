#pragma once
#include <async/Task.h>
#include <async/Tick.h>
#include <async/LinkedList.h>
#include <async/Callbacks.h>

namespace async { 
    /**
     * @class Executor
     * @brief Manages and executes asynchronous tasks and tickable objects
     * 
     * @details The Executor class is a central component for managing asynchronous operations.
     * It maintains a list of Tick objects and handles their execution according to their
     * specific timing requirements. The class provides various methods to schedule different
     * types of tasks with different timing behaviors.
     * 
     * @note Inherits from Tick, allowing executors to be nested within other executors.
     */
    class Executor : public Tick {
        private:
            LinkedList<Tick*> list; ///< List of managed Tick objects
            bool begin = false;

        public:
            bool init() {
                for(int i=0, size=list.size(); i < size; i++) {
                    Tick * tick = list.get(i);

                    if(!tick->init()) {
                        return false;
                    }
                }

                this->begin = true;
                return true;
            }
            /**
             * @brief Add a Tick object to the executor's management
             * @param tick Pointer to the Tick object to be added
             * 
             * @note The executor will call start() on the Tick object upon addition
             * @note The executor takes ownership of the Tick object's lifecycle
             */
            void add(Tick * tick) {
                list.append(tick);

                if(this->begin) {
                    tick->init();
                }
            }

            // TODO
            // void add(Task * ...) {
            // }

            /**
             * @brief Remove a Tick object from the executor's management
             * @param tick Pointer to the Tick object to be removed
             * 
             * @note The executor will call cancel() on the Tick object upon removal
             */
            void remove(Tick * tick) {
                list.remove(tick);
                tick->cancel();
                delete tick;
            }

            /**
             * @brief Execute one iteration of the executor's tick cycle
             * @return bool Always returns true to indicate the executor should continue running
             * 
             * @details Iterates through all managed Tick objects and calls their tick() method.
             * If a Tick's tick() returns false, it is automatically removed from the executor.
             */
            bool tick() {
                for(int i=0, size=list.size(); i < size; i++) {
                    Tick * tick = list.get(i);

                    if(!tick->tick()) {
                        remove(tick);
                    }
                }

                return true;
            }

            ///@name Task Creation Methods
            ///@{

            /**
             * @brief Create and add a task that executes every tick
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             */
            Task * onTick(VoidCallback cb) {
                auto task = new Task(Task::TICK, cb);
                this->add(task);
                return task;
            }

            /**
             * @brief Create and add a repeating task
             * @param duration Pointer to Duration object specifying repeat interval
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             */
            Task * onRepeat(Duration * duration, VoidCallback cb) {
                auto task = new Task(Task::REPEAT, duration, cb);
                this->add(task);
                return task;
            }

            /**
             * @brief Create and add a repeating task with millisecond interval
             * @param duration Repeat interval in milliseconds
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             */
            Task * onRepeat(uint64_t duration, VoidCallback cb) {
                return onRepeat(new Duration(duration), cb);
            }

            /**
             * @brief Create and add a delayed one-time task
             * @param duration Pointer to Duration object specifying delay time
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             */
            Task * onDelay(Duration * duration, VoidCallback cb) {
                auto task = new Task(Task::DELAY, duration, cb);
                this->add(task);
                return task;
            }

            /**
             * @brief Create and add a delayed one-time task with millisecond delay
             * @param duration Delay time in milliseconds
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             */
            Task * onDelay(uint64_t duration, VoidCallback cb) {
                return onDelay(new Duration(duration), cb);
            }
            
            /**
             * @brief Create and add a demand-based task
             * @param cb Callback function to execute
             * @return Task* Pointer to the created Task object
             * 
             * @note Demand tasks only execute when manually triggered
             */
            Task * onDemand(VoidCallback cb) {
                auto task = new Task(Task::DEMAND, cb);
                this->add(task);
                return task;
            }

            ///@}
            /**
             * @brief Create and add an interrupt-based task
             * @param pin GPIO pin number to monitor
             * @param mode Interrupt mode (RISING, FALLING, etc.)
             * @param cb Callback function to execute on interrupt
             * @return Task* Pointer to the created Task object
             * 
             * @warning Currently implemented as DEMAND task (needs proper interrupt handling)
             */
            // Task * onInterrupt(uint8_t pin, int mode, voidCallback cb) {
            //     auto task = new Task(pin, mode, cb);
            //     this->add(task);
            //     return task;
            // }
            ///@}
    };
}