#pragma once
#include <async/Tick.h>
#include <async/Time.h>
#include <async/Duration.h>
#include <async/Function.h>

/**
 * @class Task
 * @brief Task management class for scheduling and controlling asynchronous operations
 * 
 * @details The Task class provides functionality to create and manage different types
 * of timed or event-driven tasks. It supports various task types including repeating,
 * delayed, and demand-based tasks. Tasks can be paused, resumed, canceled, or reset.
 */
namespace async { 
    typedef Function<void()> voidCallback; ///< Callback function type for task execution

    class Task : public Tick {
        private:
            int type;           ///< Task type (REPEAT, DELAY, etc.)
            int state;          ///< Current state (PAUSE, RUN, CANCEL)
            Duration * duration;///< Duration for timed tasks
            Duration * from;    ///< Timestamp when task started or was last reset
            voidCallback callback; ///< Callback function to execute

        public:
            ///@name Task Type Constants
            ///@{
            static int const REPEAT = 0;      ///< Repeating task type
            static int const DELAY = 1;       ///< One-time delayed task type
            static int const DELAY_REPEAT = 2;///< Delayed repeating task type (unused)
            static int const TICK = 3;        ///< Execute every tick task type
            static int const DEMAND = 4;      ///< Demand-based task type
            ///@}

            ///@name Task State Constants
            ///@{
            static int const PAUSE = 0;  ///< Task is paused
            static int const RUN = 1;    ///< Task is running
            static int const CANCEL = 2; ///< Task is canceled
            ///@}

            /**
             * @brief Destructor
             */
            ~Task() {
                Serial.println("~Task");
                //delete duration;
                delete from;
            }

            /**
             * @brief Construct a demand-based Task
             * @param type Must be DEMAND
             * @param callback Function to execute when demanded
             */
            Task(const int type, voidCallback callback) {
                this->type = DEMAND;
                this->state = PAUSE;
                this->callback = callback;
            }

            /**
             * @brief Construct a timed Task
             * @param type Task type (REPEAT, DELAY, etc.)
             * @param duration Pointer to Duration object for task timing
             * @param callback Function to execute when task triggers
             */
            Task(const int type, Duration * duration, voidCallback callback) {
                this->type = type;
                this->duration = duration;
                this->from = new Duration(millis64());
                this->callback = callback;
            }
            
            ///@name Task Control Methods
            ///@{
            
            /**
             * @brief Start a timed task
             * @return true if successful, false for DEMAND tasks
             */
            bool start() {
                if(this->type != DEMAND) {
                    this->state = RUN;
                    return true;
                }
                return false;
            }

            /**
             * @brief Pause the task
             * @return Always returns true
             */
            bool pause() {
                this->state = PAUSE;
                return true;
            }

            /**
             * @brief Resume a paused task
             * @return Always returns true
             */
            bool resume() {
                this->state = RUN;
                return true;
            }

            /**
             * @brief Cancel the task
             * @return Always returns true
             */
            bool cancel() {
                this->state = CANCEL;
                return true;
            }

            /**
             * @brief Trigger a DEMAND task
             * @return Always returns true
             */
            bool demand() {
                this->state = RUN;
                return true;
            }

            /**
             * @brief Reset the task timer
             * @return Always returns true
             */
            bool reset() {
                this->from->set(millis64());
                return true;
            }
            ///@}

            /**
             * @brief Execute task tick logic
             * @return true if task should continue, false if task should be removed
             * 
             * @details Handles task execution based on type and state:
             * - TICK tasks execute every call
             * - DEMAND tasks execute once then pause
             * - TIMED tasks check duration before executing
             * - REPEAT tasks auto-reset after execution
             */
            bool tick() {
                if(this->state == Task::RUN) {
                    if(this->type == Task::TICK) {
                        this->callback();
                    }
                    else if(this->type == Task::DEMAND) {
                        this->callback();
                        this->pause();
                    }
                    else if(millis64() - this->from->get() > this->duration->get() && 
                           (this->type == Task::DELAY || this->type == Task::REPEAT)) {
                        this->callback();

                        if(this->type == Task::REPEAT) {
                            this->reset();
                        }
                        else {
                            this->cancel();
                            return false;
                        }
                    }
                }
                else if(this->state == Task::CANCEL) {
                    return false;
                }

                return true;
            }

            IRAM_ATTR static void isr(void* arg) {
                Task *ptr = (Task*) arg;
                ptr->demand();
            }
    };
}