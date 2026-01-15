#pragma once 

#include "driver/gpio.h"
#include "soc/rtc.h"
#include <vector>
#include <functional>
#include <async/Definitions.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {
    // Предполагаем, что эти классы уже определены
    class Pin;

    class Interrupt {
        public: 
        gpio_num_t pinNum;
        Pin * pin;
        Mode sleepMode;
        gpio_int_type_t type;
        std::function<void(Interrupt *)> callback;

            // Конструктор
        Interrupt(Pin * pin, gpio_int_type_t intType, Mode sleepMode, std::function<void(Interrupt *)> cb);
        void cancel();
        ~Interrupt();
    };

    Mode interruptLevel = Mode::None;
    std::vector<Interrupt *> globalInterruptParams;
    RTC_DATA_ATTR bool deepInterruptsRevert = false;
    RTC_DATA_ATTR int deepInterruptsMode = 0;

    uint64_t pinsMask = 0;

    void setInterruptLevel(Mode level) {
        interruptLevel = level;
    }
}