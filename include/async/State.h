#pragma once
#include <async/Log.h>
#include <async/Task.h>
#include <vector>
#include <functional>

/**
 * @file State.h
 * @brief Defines the async::State class template for variable state management.
 */

/**
 * @brief Class for storing the state of a variable.
 * 
 * The constructor takes the variable type and its initial value (if any).
 * The class provides methods for getting the state, updating the state, and change event callbacks.
 * Additional methods for state management are also available.
 */

namespace async { 
    /**
     * @brief Template class for managing the state of a variable.
     * 
     * Provides methods for getting, setting, and tracking changes to the variable.
     * Supports registering callbacks for state changes and custom get/set logic.
     * 
     * @tparam T Type of the variable.
     */
    template<typename T>
    class State : public Tick {
        protected:
            T currValue; ///< Current value of the variable.
            T prevValue; ///< Previous value before last change.
            Task * task; ///< Task to trigger change callbacks.

            /**
             * @brief Callback type for change events (new value, previous value).
             */
            typedef std::function<void(T, T)> OnChangeAllArgsCallback;

            /**
             * @brief Callback type for custom get/set logic.
             */
            typedef std::function<T(T)> GetAndSetAllArgsCallback;

        private:
            std::vector<OnChangeAllArgsCallback> callbacks; ///< List of change callbacks.

        public:
            /**
             * @brief Construct a new State object.
             * @param value Initial value.
             */
            State(T value) : currValue(value) {
                task = new Task(Task::DEMAND, [&] () {
                    for(int i=0; i < callbacks.size(); i++) {
                        callbacks[i](currValue, prevValue);
                    }
                });
            }

            /**
             * @brief Register a callback for state changes.
             * @param cbCallback Callback function.
             */
            void onChange(OnChangeAllArgsCallback cbCallback) { 
                callbacks.push_back(cbCallback);
            }

            /**
             * @brief Get and set value using a callback.
             * @param cbCallback Callback function.
             */
            void getAndSet(GetAndSetAllArgsCallback cbCallback) {
                this->set(cbCallback(this->currValue));
            }

            /**
             * @brief Get the current value.
             * @return Current value.
             */
            virtual T get() {
                return this->currValue;
            }

            /**
             * @brief Set the value and trigger change callbacks.
             * @param value New value.
             * @param force Force update even if value is unchanged.
             */
            virtual void set(T value, bool force = false) {
                if(this->currValue != value || force) {
                    this->prevValue = this->currValue;
                    this->currValue = value;
                    task->demand();
                }
            }

            /**
             * @brief Tick handler for the state.
             * @return true if successful.
             */
            bool tick() override {
                return task->tick();
            };
    };
}