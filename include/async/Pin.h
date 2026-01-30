#pragma once

#include <functional>
#include <vector>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "esp_heap_trace.h"

#include <async/Task.h>
#include <async/Interrupt.h>


// Метки для шаблонов прерываний
namespace async {
    class Pin {
        
    private:
        gpio_num_t pinNum;
        uint8_t currentMode;
        
        // Ресурсы LEDC (PWM / Tone)
        ledc_channel_t ledChan = LEDC_CHANNEL_MAX;
        ledc_timer_t timerNum = LEDC_TIMER_MAX;
        bool isLedcActive = false;
        bool revert = false;
        bool interrupted = false;
        int level;
        std::vector<Interrupt *> interruptParams;
        Task * interruptTask = nullptr;
        // Ресурсы АЦП
        static adc_oneshot_unit_handle_t adc1Handle;
        static bool adcUnitInit;

        void interrupt();

        Interrupt * addInterrupt(Mode mode, gpio_int_type_t type, std::function<void(Interrupt *)> callback);

        void stopLedcInternal();

        adc_channel_t getAdcChannelInternal();

    public:
        Pin(int pin, int mode = INPUT_PULLUP, int defaultLevel = HIGH);

        gpio_num_t getPin();

        uint8_t getMode();

        void setMode(uint8_t mode);

        // --- Цифровой функционал ---
        void digitalWrite(int level, bool disableModeCheck = false);

        int digitalRead();

        // --- Аналоговый функционал ---
        int analogRead();

        void analogWrite(int duty, bool disableModeCheck = false);

        bool isReverted() {
            return revert;
        }

        void tone(uint32_t freq, Duration * duration = nullptr);

        void noTone();

        // --- Прерывания и Сон ---
        template<Mode mode>
        Interrupt * addInterrupt(gpio_int_type_t type, std::function<void(Interrupt *)> callback) {
            return addInterrupt(mode, type, callback);
        }
        
        void removeInterrupt(Interrupt * task);

        static void ISR(void* arg);
    };

}