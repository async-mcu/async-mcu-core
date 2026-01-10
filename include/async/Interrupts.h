#pragma once 

#include "driver/gpio.h"
#include <vector>
#include <async/Mode.h>
#include <async/Task.h>

#define INTERRUPT_DIRECT true
#define INTERRUPT_SCHEDULE false

namespace async {

Mode interruptLevel = Mode::None;

typedef struct {
    gpio_int_type_t type;
    std::function<void(void)> callback;
} interrupt_params;
// Прерывания

std::vector<interrupt_params> interrupts;


void setInterruptLevel(Mode level) {
    interruptLevel = level;
}

}