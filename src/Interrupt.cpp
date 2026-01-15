#pragma once

#include <async/Interrupts.h>
#include <async/Pin.h>

using namespace async;

Interrupt::Interrupt(Pin *p, gpio_int_type_t intType, Mode sleep, 
                            std::function<void(Interrupt *)> cb)
        : pin(p), type(intType), sleepMode(sleep), callback(cb) {
        // ESP_LOGD(TAG_INTERRUPT, "Create interrupt pin %d, type: %d, pinMode: %d, sleepMode %s", 
        //         pin->getPin(), type, pin->getMode(), modeToStr(sleepMode));
    }

 Interrupt::~Interrupt() {
        pin->removeInterrupt(this);
        // ESP_LOGD(TAG_INTERRUPT, "Delete interrupt pin %d, type: %d, sleepMode: %s", 
        //         pin ? pin->getPin() : -1, type, modeToStr(sleepMode));
    }

void Interrupt::cancel() {
        pin->removeInterrupt(this);
        // ESP_LOGD(TAG_INTERRUPT, "Cancel interrupt on pin %d", pin->getPin());
    }