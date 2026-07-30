#pragma once

#include <functional>
#include <vector>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "esp_heap_trace.h"

#include <async/Task.h>
#include <async/Interrupt.h>


namespace async {
    class Pin {
        
    private:
        gpio_num_t pinNum;
        uint8_t currentMode;
        SleepMode sleepMode = SleepMode::None;
        
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

        // Отложенное планирование прерываний из ISR (без malloc в ISR)
        static QueueHandle_t irqQueue;
        static TaskHandle_t irqTask;
        static void ensureIrqDispatch();
        static void irqDispatchTask(void * arg);

        void stopLedcInternal();
        adc_channel_t getAdcChannelInternal();

        void IRAM_ATTR interrupt();
        Interrupt * addInterrupt(SleepMode sleepMode, gpio_int_type_t type, std::function<void(Interrupt &)> callback);
    public:
        Pin(int pin, int mode = INPUT_PULLUP, int defaultLevel = HIGH);
        gpio_num_t IRAM_ATTR getPin();
        uint8_t getMode();
        void setMode(uint8_t mode);
        void digitalWrite(int level, bool disableModeCheck = false);
        int IRAM_ATTR digitalRead(bool setAutoMode = true);
        int analogRead();
        void analogWrite(int duty, bool disableModeCheck = false);
        bool isReverted();
        void tone(uint32_t freq, Duration * duration = nullptr);
        void noTone();
        void removeInterrupt(Interrupt * task);
        static void IRAM_ATTR ISR(void* arg);

        // --- Прерывания 
        template<SleepMode sleepMode>
        Interrupt * addInterrupt(gpio_int_type_t type, std::function<void(Interrupt &)> callback) {
            return addInterrupt(sleepMode, type, callback);
        }

        template<SleepMode sleepMode>
        Interrupt * addInterrupt(int type, std::function<void(Interrupt &)> callback) {
            return addInterrupt(sleepMode, (gpio_int_type_t)type, callback);
        }
    };

}