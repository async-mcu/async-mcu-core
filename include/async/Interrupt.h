#pragma once 

#include "driver/gpio.h"
#include "soc/rtc.h"
#include <vector>
#include <functional>
#include <async/Definitions.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {
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

    void setInterruptLevel(Mode level);
    Mode getInterruptLevel();
    void setDeepInterruptsRevert(bool revert);
    bool getDeepInterruptsRevert();
    void setDeepInterruptsMode(int mode);
    int getDeepInterruptsMode();
    std::vector<Interrupt *> getGlobalInterruptParams();
}