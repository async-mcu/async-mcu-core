#pragma once
#include <async/Tick.h>
#include <async/Time.h>
#include <async/Duration.h>
#include <async/Callbacks.h>
#include <esp_timer.h>

/**
 * @file Task.h
 * @brief Defines task classes for scheduling and controlling asynchronous operations.
 */

namespace async {

    enum TaskState {
        CREATE = 0, ///< Object created
        RUN    = 1, ///< Object started
        CANCEL = 2, ///< Object canceled
        PAUSE  = 3  ///< Object paused
    };

    /**
     * @class DemandTask
     * @brief Task triggered on demand, supports typed parameter and result in callback.
     *
     * ### Example: DemandTask with int parameter
     * ```cpp
     * auto demand1 = new DemandTask<int>([](int result) {
     *     Serial.println(result); // Prints 123
     * });
     * demand1->start();
     * demand1->demand(123);
     * ```
     *
     * ### Example: DemandTask with no parameter
     * ```cpp
     * auto demand2 = new DemandTask<>([] () {
     *     Serial.println("No param demand!");
     * });
     * demand2->start();
     * demand2->demand();
     * ```
     */
    template<typename ParamT = void>
    class DemandTask : public Tick {
        protected:
            volatile TaskState state;
            std::function<void(ParamT)> callback;
            ParamT param;

        public:
            DemandTask(std::function<void(ParamT)> cb) : state(CREATE), callback(cb), param() {}

            void attach(Executor * executor) {
                executor->add(this);
            }
            bool start() override { state = RUN; return true; }
            bool pause() override { state = PAUSE; return true; }
            bool resume() override { state = RUN; return true; }
            bool cancel() override { state = CANCEL; return true; }

            /**
             * @brief Demand execution with parameter
             * @param value Parameter for callback
             */
            void demand(ParamT value) {
                param = value;
                if (state == RUN && callback) {
                    callback(param);
                    state = PAUSE;
                }
            }

            void setParam(const ParamT& value) { param = value; }
            ParamT getParam() const { return param; }
            bool tick() override { return state != CANCEL; }
        };

        // Specialization for void parameter
        template<>
        class DemandTask<void> : public Tick {
        protected:
            volatile TaskState state;
            std::function<void()> callback;

        public:
            DemandTask(std::function<void()> cb) : state(CREATE), callback(cb) {}

            bool start() override { state = RUN; return true; }
            bool pause() override { state = PAUSE; return true; }
            bool resume() override { state = RUN; return true; }
            bool cancel() override { state = CANCEL; return true; }

            /**
             * @brief Demand execution without parameter
             */
            void demand() {
                if (state == RUN && callback) {
                    callback();
                    state = PAUSE;
                }
            }

            bool tick() override { return state != CANCEL; }
    };

    /**
     * @class TickTask
     * @brief Task executed every tick
     */
    class TickTask : public Tick {
    protected:
        volatile TaskState state;
        VoidCallback callback;

    public:
        TickTask(VoidCallback cb) : state(CREATE), callback(cb) {}

        bool start() override { state = RUN; return true; }
        bool pause() override { state = PAUSE; return true; }
        bool resume() override { state = RUN; return true; }
        bool cancel() override { state = CANCEL; return true; }

        bool tick() override {
            if (state == RUN) callback();
            return state != CANCEL;
        }
    };

    /**
     * @class DelayTask
     * @brief One-time delayed task using esp_timer
     */
    class DelayTask : public Tick {
    protected:
        volatile TaskState state;
        Duration* duration;
        VoidCallback callback;
        esp_timer_handle_t timer;

        static void timerCallback(void* arg) {
            DelayTask* self = static_cast<DelayTask*>(arg);
            if (self->state == RUN && self->callback) {
                self->callback();
                self->state = CANCEL;
            }
        }

    public:
        DelayTask(Duration* dur, VoidCallback cb) : state(CREATE), duration(dur), callback(cb), timer(nullptr) {}

        bool start() override {
            if (state == CREATE || state == PAUSE) {
                esp_timer_create_args_t args = {
                    .callback = &DelayTask::timerCallback,
                    .arg = this,
                    .dispatch_method = ESP_TIMER_TASK,
                    .name = "D" // DelayTask
                };
                esp_timer_create(&args, &timer);
                esp_timer_start_once(timer, duration->get(Duration::MICRO));
                state = RUN;
            }
            return true;
        }

        bool pause() override {
            if (timer) esp_timer_stop(timer);
            state = PAUSE;
            return true;
        }

        bool resume() override {
            if (timer) esp_timer_start_once(timer, duration->get(Duration::MICRO));
            state = RUN;
            return true;
        }

        bool cancel() override {
            if (timer) {
                esp_timer_stop(timer);
                esp_timer_delete(timer);
                timer = nullptr;
            }
            state = CANCEL;
            return true;
        }

        bool tick() override { return state != CANCEL; }
    };

    /**
     * @class RepeatTask
     * @brief Repeating task using esp_timer
     */
    class RepeatTask : public Tick {
    protected:
        volatile TaskState state;
        Duration* duration;
        VoidCallback callback;
        esp_timer_handle_t timer;

        static void timerCallback(void* arg) {
            RepeatTask* self = static_cast<RepeatTask*>(arg);
            if (self->state == RUN && self->callback) {
                self->callback();
            }
        }

    public:
        RepeatTask(Duration* dur, VoidCallback cb) : state(CREATE), duration(dur), callback(cb), timer(nullptr) {}

        bool start() override {
            if (state == CREATE || state == PAUSE) {
                esp_timer_create_args_t args = {
                    .callback = &RepeatTask::timerCallback,
                    .arg = this,
                    .dispatch_method = ESP_TIMER_TASK,
                    .name = "R" // RepeatTask
                };
                esp_timer_create(&args, &timer);
                esp_timer_start_periodic(timer, duration->get(Duration::MICRO));
                state = RUN;
            }
            return true;
        }

        bool pause() override {
            if (timer) esp_timer_stop(timer);
            state = PAUSE;
            return true;
        }

        bool resume() override {
            if (timer) esp_timer_start_periodic(timer, duration->get(Duration::MICRO));
            state = RUN;
            return true;
        }

        bool cancel() override {
            if (timer) {
                esp_timer_stop(timer);
                esp_timer_delete(timer);
                timer = nullptr;
            }
            state = CANCEL;
            return true;
        }

        bool tick() override { return state != CANCEL; }
    };
}