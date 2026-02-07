#pragma once

#include <vector>
#include <esp_attr.h>
#include <esp_system.h>
#include <functional>
#include <async/Task.h>

extern "C" {
    extern uint32_t _rtc_noinit_start;
    extern uint32_t _rtc_noinit_end;
    extern uint32_t _rtc_data_start;
    extern uint32_t _rtc_data_end;
}

namespace async {

bool isRtcDataVariable(const void* ptr) {
    return (uint32_t)ptr >= (uint32_t)&_rtc_data_start && 
           (uint32_t)ptr <= (uint32_t)&_rtc_data_end;
}

bool isRtcNoInitVariable(const void* ptr) {
    return (uint32_t)ptr >= (uint32_t)&_rtc_noinit_start && 
           (uint32_t)ptr <= (uint32_t)&_rtc_noinit_end;
}

template<typename T>
class State {
    protected:
        T currValue; ///< Current value of the variable.
        T prevValue; ///< Previous value before last change.
        std::vector<std::function<void(T, T)>> callbacks;

    public:
        State(T value) {
            if(!isRtcDataVariable(this) || esp_reset_reason() != ESP_RST_DEEPSLEEP) {
                currValue = value;
                prevValue = value;
            }
        }

        void onChange(std::function<void(T, T)> callback) {
            callbacks.push_back(callback);
        }

        void set(T value) {
            for(std::function<void(T, T)> callback : callbacks) {
                onOnce([prev = this->currValue, neww = value, callback] (Task *) {
                    callback(prev, neww);
                });
            }
            
            this->prevValue = this->currValue;
            this->currValue = value;
        }

        /**
         * @brief Get and set value using a callback.
         * @param cbCallback Callback function.
         */
        T set(std::function<T(T)> cbCallback) {
            this->set(cbCallback(this->currValue));
            return this->currValue;
        }

        /**
         * @brief Get the current value.
         * @return Current value.
         */
        virtual T get() {
            return this->currValue;
        }

        bool isNotNull() {
            return currValue != nullptr;
        }

        void setNull() {
            set(nullptr);
        }
};

}