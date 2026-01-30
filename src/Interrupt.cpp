#include <async/Pin.h>
#include <async/Interrupt.h>
#include <async/Logging.h>

namespace async {

    // Global variable definitions from Interrupt.h
    Mode interruptLevel = Mode::None;
    RTC_DATA_ATTR bool deepInterruptsRevert = false;
    RTC_DATA_ATTR int deepInterruptsMode = 0;
    std::vector<Interrupt *> globalInterruptParams;

    // Interrupt class implementations (moved from Pin.cpp)
    Interrupt::Interrupt(Pin *p, gpio_int_type_t intType, Mode sleep, std::function<void(Interrupt *)> cb)
        : pin(p), type(intType), sleepMode(sleep), callback(cb) {
        ESP_LOGD(TAG_INTERRUPT, "Create interrupt pin %d, type: %d, pinMode: %d, sleepMode %s", pin->getPin(), type, pin->getMode(), modeToStr(sleepMode));
    }

    Interrupt::~Interrupt() {
        pin->removeInterrupt(this);
        ESP_LOGD(TAG_INTERRUPT, "Delete interrupt pin %d, type: %d, sleepMode: %s", pin ? pin->getPin() : -1, type, modeToStr(sleepMode));
    }

    void Interrupt::cancel() {
        ESP_LOGD(TAG_INTERRUPT, "Cancel interrupt pin %d", pin->getPin());
        pin->removeInterrupt(this);
        ESP_LOGD(TAG_INTERRUPT, "Cancel interrupt on pin %d", pin->getPin());
    }

    void setInterruptLevel(Mode level) {
        interruptLevel = level;
    }

    void setDeepInterruptsRevert(bool revert) {
        deepInterruptsRevert = revert;
    }

    bool getDeepInterruptsRevert() {
        return deepInterruptsRevert;
    }

    void setDeepInterruptsMode(int mode) {
        deepInterruptsMode = mode;
    }

    int getDeepInterruptsMode() {
        return deepInterruptsMode;
    }

    Mode getInterruptLevel() {
        return interruptLevel;
    }

    std::vector<Interrupt *> getGlobalInterruptParams() {
        return globalInterruptParams;
    }
}