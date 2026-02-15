#include <async/Pin.h>
#include <async/Interrupt.h>
#include <async/Logging.h>
#include <algorithm>

namespace async {

    // Global variable definitions from Interrupt.h
    SleepMode interruptSleepMode = SleepMode::None;
    RTC_DATA_ATTR bool deepInterruptsRevert = false;
    RTC_DATA_ATTR int deepInterruptsMode = 0;
    std::vector<Interrupt *> globalInterruptParams;

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


    SleepMode getInterruptSleepMode() {
        return interruptSleepMode;
    }

    void setInterruptSleepMode(SleepMode sleepMode) {
        interruptSleepMode = sleepMode;
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

    const std::vector<Interrupt *> & getGlobalInterruptParams() {
        return globalInterruptParams;
    }

    void addGlobalInterruptParam(Interrupt * p) {
        if (!p) return;
        if (std::find(globalInterruptParams.begin(), globalInterruptParams.end(), p) == globalInterruptParams.end()) {
            globalInterruptParams.push_back(p);
        }
    }

    void removeGlobalInterruptParam(Interrupt * p) {
        auto it = std::find(globalInterruptParams.begin(), globalInterruptParams.end(), p);
        if (it != globalInterruptParams.end()) {
            globalInterruptParams.erase(it);
        }
    }
}