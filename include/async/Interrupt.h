#pragma once 

#include "driver/gpio.h"
#include "soc/rtc.h"
#include <vector>
#include <functional>
#include <async/Globals.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {
    class Pin; 

    class Interrupt {
        public: 
        Pin & pin;
        SleepMode sleepMode;
        gpio_int_type_t type;
        std::function<void(Interrupt &)> callback;

            // Конструктор
        Interrupt(Pin & pin, gpio_int_type_t intType, SleepMode sleepMode, std::function<void(Interrupt &)> cb);
        void cancel();
        ~Interrupt();
    };
}