#include <async/Pin.h>
#include <async/Interrupt.h>
#include <async/Logging.h>
#include <algorithm>

namespace async {

    // Interrupt class implementations (moved from Pin.cpp)
    Interrupt::Interrupt(Pin &p, gpio_int_type_t intType, SleepMode sleepMode, std::function<void(Interrupt &)> cb)
        : pin(p), type(intType), sleepMode(sleepMode), callback(cb) {
        ESP_LOGD(TAG_INTERRUPT, "Create interrupt pin %d, type: %d, pinMode: %d, sleepMode %s", pin.getPin(), type, pin.getMode(), modeToStr(sleepMode));
    }

    Interrupt::~Interrupt() {
        pin.removeInterrupt(this);
        ESP_LOGD(TAG_INTERRUPT, "Delete interrupt pin %d, type: %d, sleepMode: %s", pin.getPin(), type, modeToStr(sleepMode));
    }

    void Interrupt::cancel() {
        ESP_LOGD(TAG_INTERRUPT, "Cancel interrupt pin %d", pin.getPin());
        pin.removeInterrupt(this);
        ESP_LOGD(TAG_INTERRUPT, "Cancel interrupt on pin %d", pin.getPin());
    }
}