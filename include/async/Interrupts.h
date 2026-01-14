#pragma once 

#include "driver/gpio.h"
#include "soc/rtc.h"
#include <vector>
#include <functional>
#include <async/Definitions.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {

    Mode interruptLevel = Mode::None;

    // Прерывания
    typedef struct {
        gpio_int_type_t type;
        std::function<void(void)> callback;
        gpio_num_t pinNum;
        uint8_t pinMode;
        Mode sleepMode;
    } interrupt_params;

    std::vector<interrupt_params> globalInterrupts;
    RTC_DATA_ATTR bool deepInterruptsRevert = false;
    RTC_DATA_ATTR int deepInterruptsMode = 0;
    int deepInterruptsRevertCount = 0;

    uint64_t pinsMask = 0;

    void setInterruptLevel(Mode level) {
        interruptLevel = level;
    }
}