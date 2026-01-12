#pragma once 

#include "driver/gpio.h"
#include <vector>
#include <functional>
#include <async/Definitions.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {

    Mode interruptLevel = Mode::None;

    typedef struct {
        gpio_int_type_t type;
        std::function<void(void)> callback;
        gpio_num_t pinNum;
    } interrupt_params;
    // Прерывания

    std::vector<interrupt_params> globalInterrupts;


    void setInterruptLevel(Mode level) {
        interruptLevel = level;
    }

}