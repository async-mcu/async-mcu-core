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
        Pin * pin;
        SleepMode sleepMode;
        gpio_int_type_t type;
        std::function<void(Interrupt *)> callback;

            // Конструктор
        Interrupt(Pin * pin, gpio_int_type_t intType, SleepMode sleepMode, std::function<void(Interrupt *)> cb);
        void cancel();
        ~Interrupt();
    };

    void setInterruptSleepMode(SleepMode level);
    SleepMode getInterruptSleepMode();
    void setDeepInterruptsRevert(bool revert);
    bool getDeepInterruptsRevert();
    void setDeepInterruptsMode(int mode);
    int getDeepInterruptsMode();
    const std::vector<Interrupt *> & getGlobalInterruptParams();
    void addGlobalInterruptParam(Interrupt * p);
    void removeGlobalInterruptParam(Interrupt * p);
}