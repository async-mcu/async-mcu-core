#pragma once

#include <functional>
#include <vector>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "esp_heap_trace.h"
#include <async/Executor.h>
#include <async/Interrupts.h>


// Метки для шаблонов прерываний
namespace async {

const int validDeepSleepPins[] = {0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39};
bool isValidRtcPin(int pin) {
    for(int i = 0; i < sizeof(validDeepSleepPins)/sizeof(validDeepSleepPins[0]); i++) {
        if(pin == validDeepSleepPins[i]) return true;
    }
    return false;
}

extern std::vector<interrupt_params> globalInterrupts;
extern uint64_t pinsMask;
extern int deepInterruptsMode;
//extern std::vector<interrupt_params> interrupts;
bool isrServiceInstalled = false;

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
    int globalInterruptNumber;
    std::vector<interrupt_params> interrupts;
    Task * interruptTask = nullptr;

    // Ресурсы АЦП
    static adc_oneshot_unit_handle_t adc1Handle;
    static bool adcUnitInit;

    void interrupt() {
        int value = digitalRead();

        if(!interrupted) {
            //ets_printf("!interrupted %d!  \n", pinNum);
            interrupted = true;
            interruptTask->setValue((void *) value);
            interruptTask->schedule();
        }
    }

    void stopLedcInternal() {
        if (isLedcActive) {
            ledc_stop(LEDC_LOW_SPEED_MODE, ledChan, 0);
            isLedcActive = false;
        }
    }

    adc_channel_t getAdcChannelInternal() {
        switch (pinNum) {
            case 36: return ADC_CHANNEL_0; case 37: return ADC_CHANNEL_1;
            case 38: return ADC_CHANNEL_2; case 39: return ADC_CHANNEL_3;
            case 32: return ADC_CHANNEL_4; case 33: return ADC_CHANNEL_5;
            case 34: return ADC_CHANNEL_6; case 35: return ADC_CHANNEL_7;
            default: return (adc_channel_t)-1;
        }
    }

public:
    Pin(int pin, int mode = INPUT_PULLUP, int defaultLevel = HIGH): pinNum((gpio_num_t)pin), currentMode(mode), level(defaultLevel) {
        onOnce(CURRENT_CORE, [this](Task *) {
            ESP_LOGD(TAG_PIN, "Init pin %d, mode %d", pinNum, currentMode);

            setMode(currentMode);

            switch (currentMode) {
                case OUTPUT:
                    digitalWrite(level, true);
                    break;
                case OUTPUT_ANALOG:
                    analogWrite(level, true);
                    break;
            }
        });
    };

    inline gpio_num_t getPin() {
        return pinNum;
    }

    uint8_t getMode() {
        return currentMode;
    }

    void setMode(uint8_t mode) {
        stopLedcInternal();
        currentMode = mode;
        gpio_set_pull_mode(pinNum, GPIO_FLOATING);

        switch (mode) {
            case INPUT:
                gpio_set_direction(pinNum, GPIO_MODE_INPUT);
                break;
            case OUTPUT | OUTPUT_ANALOG:
                gpio_set_direction(pinNum, GPIO_MODE_INPUT_OUTPUT);
                break;
            case INPUT_PULLUP:
                gpio_set_direction(pinNum, GPIO_MODE_INPUT);
                gpio_set_pull_mode(pinNum, GPIO_PULLUP_ONLY);
                break;
            case INPUT_PULLDOWN:
                gpio_set_direction(pinNum, GPIO_MODE_INPUT);
                gpio_set_pull_mode(pinNum, GPIO_PULLDOWN_ONLY);
                break;
            case ANALOG:
                gpio_set_direction(pinNum, GPIO_MODE_DISABLE);
                break;
            case OUTPUT_OPEN_DRAIN:
                gpio_set_direction(pinNum, GPIO_MODE_OUTPUT_OD);
                break;
        }
    }

    // --- Цифровой функционал ---
    void digitalWrite(int level, bool disableModeCheck = false) {
        if (!disableModeCheck && !(currentMode & 0x02)) setMode(OUTPUT);
        gpio_set_level(pinNum, level);
        this->level = level;
    }

    int digitalRead() {
        if (currentMode == ANALOG) setMode(INPUT);
        return gpio_get_level(pinNum);
    }

    // --- Аналоговый функционал ---
    int analogRead() {
        if (currentMode != ANALOG) setMode(ANALOG);
        if (!adcUnitInit) {
            adc_oneshot_unit_init_cfg_t i_cfg = { ADC_UNIT_1 };
            adc_oneshot_new_unit(&i_cfg, &adc1Handle);
            adcUnitInit = true;
        }
        adc_channel_t chan = getAdcChannelInternal();
        if (chan == (adc_channel_t)-1) return 0;

        adc_oneshot_chan_cfg_t c_cfg = { ADC_ATTEN_DB_12, ADC_BITWIDTH_DEFAULT };
        adc_oneshot_config_channel(adc1Handle, chan, &c_cfg);
        int val = 0;
        adc_oneshot_read(adc1Handle, chan, &val);
        return val;
    }

    void analogWrite(int duty, bool disableModeCheck = false) {
        if (!disableModeCheck && !(currentMode & 0x02)) setMode(OUTPUT);

        if (!isLedcActive) {
            static int globalChannelCounter = 0;
            if (ledChan == LEDC_CHANNEL_MAX) {
                ledChan = (ledc_channel_t)(globalChannelCounter % 8);
                timerNum = (ledc_timer_t)(globalChannelCounter % 4);
                globalChannelCounter++;
            }
            ledc_timer_config_t t_cfg = { LEDC_LOW_SPEED_MODE, LEDC_TIMER_10_BIT, timerNum, 5000, LEDC_AUTO_CLK };
            ledc_timer_config(&t_cfg);
            ledc_channel_config_t c_cfg = { pinNum, LEDC_LOW_SPEED_MODE, ledChan, LEDC_INTR_DISABLE, timerNum, 0, 0 };
            ledc_channel_config(&c_cfg);
            isLedcActive = true;
            currentMode = OUTPUT_ANALOG;
        }
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ledChan, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ledChan);
    }

    void tone(uint32_t freq, Duration * duration = nullptr) {
        if (freq == 0) { stopLedcInternal(); return; }
        analogWrite(512); 
        ledc_set_freq(LEDC_LOW_SPEED_MODE, timerNum, freq);

        if(duration != nullptr) {
            onDelay<Active>(duration, [this](Task *) {
                stopLedcInternal();
            });
        }
    }

    void noTone() {
        stopLedcInternal();
    }

    bool isWakeupReason() {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_EXT0) return true;
        if (cause == ESP_SLEEP_WAKEUP_EXT1) {
            return (esp_sleep_get_ext1_wakeup_status() & (1ULL << pinNum));
        }
        if (cause == ESP_SLEEP_WAKEUP_GPIO) return true;
        return false;
    }

    // --- Прерывания и Сон ---
    template<Mode mode>
    void onInterrupt(gpio_int_type_t type, std::function<void(void)> callback); 
    static void ISR(void* arg);
};

inline void Pin::ISR(void* arg) {
    Pin *instance = (Pin*) arg;
    gpio_intr_disable(instance->getPin());
    ets_printf("ISR pin %d!\n", instance->getPin());
    instance->interrupt();
}

// Реализация в конце файла
template<Mode mode>
inline void Pin::onInterrupt(gpio_int_type_t type, std::function<void(void)> callback) {
    // add callback to list
    interrupts.push_back((interrupt_params) {
        .type = type,
        .callback = callback
    });

    globalInterruptNumber = globalInterrupts.size();
    globalInterrupts.push_back((interrupt_params) {
        .type = type,
        .pinNum = pinNum,
        .pinMode = currentMode,
        .sleepMode = mode
    });

    // first install isr service
    if (!isrServiceInstalled) {
        gpio_install_isr_service(0);
        isrServiceInstalled = true;
    }

    // first init interrupt task for this pin
    if(interruptTask == nullptr) {
        interruptTask = onDemand([this](Task * demandTask) {
            int value = (int) demandTask->getValue();
            
            //ets_printf("onDemand!\n");

            if(mode == Mode::Deep || mode == Mode::Light) {
                revert = !revert;

                if(revert) {
                    ESP_LOGV(TAG_PIN, "gpio_wakeup_enable revert pin %d to %d", pinNum, (currentMode != INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable(pinNum, (currentMode != INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
                }
                else {
                    ESP_LOGV(TAG_PIN, "gpio_wakeup_enable pin %d to %d", pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
                    gpio_wakeup_enable(pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
                }
            }
            
            gpio_intr_enable(pinNum);

            for(auto interrupt : interrupts) {
                if(interrupt.type == ONLOW && value == LOW) {
                    onTick([interrupt, this](Task * tickTask) {
                        //ets_printf("onTick!\n");
                        int value = digitalRead();

                        if(value == HIGH) {
                            tickTask->cancel();
                        }
                        else {
                            interrupt.callback();
                        }
                    });
                }
                else if(interrupt.type == ONHIGH && value == HIGH) {
                    onTick([interrupt, this](Task * tickTask) {
                        int value = digitalRead();

                        if(value == LOW) {
                            tickTask->cancel();
                        }
                        else {
                            interrupt.callback();
                        }
                    });
                }
                else if(interrupt.type == RISING && value == HIGH) {
                    interrupt.callback();
                }
                else if(interrupt.type == FALLING && value == LOW) {
                    interrupt.callback();
                }
                else if(interrupt.type == CHANGE) {
                    interrupt.callback();
                }
            }

            interrupted = false;
        });
    }

    bool rising = false;
    bool falling = false;
    bool onlow = false;
    bool onhigh = false;
    bool anyedge = false;

    //
    for(auto interrupt : interrupts) {
        if(interrupt.type == RISING) rising = true;
        else if(interrupt.type == FALLING) falling = true;
        else if(interrupt.type == ONLOW) onlow = true;
        else if(interrupt.type == ONHIGH) onhigh = true;
        else if(interrupt.type == CHANGE) anyedge = true;
    }

    // set active interrupt
    if(anyedge || (rising && falling) || (onlow && onhigh) || (rising && onlow) || (falling && onhigh)) {
        gpio_set_intr_type(pinNum, GPIO_INTR_ANYEDGE);
        ESP_LOGD(TAG_PIN, "gpio_set_intr_type %d GPIO_INTR_ANYEDGE!", pinNum);
    }
    else if (rising || onhigh) { // programmatically onhigh
        gpio_set_intr_type(pinNum, GPIO_INTR_POSEDGE);
        ESP_LOGD(TAG_PIN, "gpio_set_intr_type %d GPIO_INTR_POSEDGE!", pinNum);
    }
    else if (falling || onlow) { // programmatically onlow
        gpio_set_intr_type(pinNum, GPIO_INTR_NEGEDGE);
        ESP_LOGD(TAG_PIN, "gpio_set_intr_type %d GPIO_INTR_NEGEDGE!", pinNum);
    }
        
    ESP_ERROR_CHECK(gpio_isr_handler_add(pinNum, ISR, (void*) this));

    //
    if(mode == Mode::Active) {
        setInterruptLevel(Mode::Active);
    }
    else if(mode == Mode::Light) {
        setInterruptLevel(Mode::Light);
        ESP_LOGD(TAG_PIN, "gpio_wakeup_enable pin %d to %d", pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable(pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
    }
    else if(mode == Mode::Deep) {
        setInterruptLevel(Mode::Deep);

        ESP_LOGD(TAG_PIN, "gpio_wakeup_enable from deep pin %d to %d", pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable(pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);

        if(!isValidRtcPin(pinNum)) {
            esp_system_abort("Pin is not RTC!");
        }

        for(interrupt_params int_params : globalInterrupts) {
            if(int_params.pinMode != currentMode && int_params.sleepMode == Mode::Deep) {
                esp_system_abort("In deep sleep mode, modes for all pins must be the same (INPUT_PULLUP or INPUTPULLDOWN)");
            }

            pinsMask |= (1ULL << int_params.pinNum);
        }
                
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        
        if (cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) {
            bool shouldInterrupt = (cause == ESP_SLEEP_WAKEUP_EXT0)
                || (currentMode == INPUT_PULLUP) 
                || (digitalRead() == (currentMode == INPUT_PULLDOWN ? HIGH : LOW)) 
                || (esp_sleep_get_ext1_wakeup_status() & (1ULL << pinNum));
            
            if (shouldInterrupt) {
                interrupt();
                deepInterruptsMode = !deepInterruptsMode;
            }
        }
    }
}

// Инициализация статики
adc_oneshot_unit_handle_t Pin::adc1Handle = nullptr;
bool Pin::adcUnitInit = false;

}


// // Хранилище конфигурации PWM
// typedef struct {
//     bool initialized;
//     uint8_t duty;
// } pwm_pin_t;


// uint8_t next_pwm_channel = 0;


// // Быстрое чтение GPIO через регистры
// bool IRAM_ATTR read_gpio_reg(uint8_t pin) {
//     if (pin < 32) {
//         return (REG_READ(GPIO_IN_REG) >> pin) & 0x1;
//     } else {
//         return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 0x1;
//     }
// }

// // Быстрая запись GPIO через регистры
// void IRAM_ATTR write_gpio_reg(uint8_t pin, bool value) {
//     if (pin < 32) {
//         if (value) {
//             REG_WRITE(GPIO_OUT_W1TS_REG, 1 << pin);
//         } else {
//             REG_WRITE(GPIO_OUT_W1TC_REG, 1 << pin);
//          }
//     } else {
//         if (value) {
//             REG_WRITE(GPIO_OUT1_W1TS_REG, 1 << (pin - 32));
//         } else {
//             REG_WRITE(GPIO_OUT1_W1TC_REG, 1 << (pin - 32));
//         }
//     }
// }

// bool IRAM_ATTR read_output_pin(uint8_t pin) {
//     bool value;
//     uint32_t pin_mask;
//     bool is_lower_bank = pin < 32;
        
//     pin_mask = is_lower_bank ? (1 << pin) : (1 << (pin - 32));
        
//     if (is_lower_bank) {
//         REG_WRITE(GPIO_ENABLE_W1TC_REG, pin_mask);
//         __asm__ volatile("nop; nop; nop; nop;");
//         value = (REG_READ(GPIO_IN_REG) >> pin) & 0x1;
//         REG_WRITE(GPIO_ENABLE_W1TS_REG, pin_mask);
//     } else {
//         REG_WRITE(GPIO_ENABLE1_W1TC_REG, pin_mask);
//         __asm__ volatile("nop; nop; nop; nop;");
//         value = (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 0x1;
//         REG_WRITE(GPIO_ENABLE1_W1TS_REG, pin_mask);
//     }
        
//     return value;
// }

// int8_t get_adc_channel(uint8_t pin) {
//     const uint8_t adc1_pins[] = {36, 37, 38, 39, 32, 33, 34, 35};
//     for (int i = 0; i < 8; i++) {
//         if (pin == adc1_pins[i]) return i;
//     }

//     return -1;
// }

// namespace async {
//     class Pin {
//         private:
//             int pin;
//             int mode;
//             int value;
//             Task * interruptTask;
//             std::vector<Task *> handlersRising;
//             std::vector<Task *> handlersFalling;
//         public:

//         Pin(int pin, int mode = INPUT_PULLUP, int defaultValue = HIGH): pin(pin), mode(mode), value(defaultValue) {
//             onDelay(0, cpu_hal_get_core_id() == 0 ? CORE0 : CORE1, [this](Task * task) {
//                  this->setMode(this->getMode());
//             });
//         };

//         static IRAM_ATTR void ISR(void* arg) {
//             Pin *ptr = (Pin*) arg;
//             ptr->notify(read_gpio_reg(ptr->digitalRead()));
//             //ets_printf("Button press\n");
//             //ptr->demand();
//         }

//         void notify(bool value) {
        
//         }

//         void digitalWrite(bool state) {
//             if (pin >= 40) return;
            
//             // Убедимся, что пин настроен как выход
//             if (!((REG_READ(pin < 32 ? GPIO_ENABLE_REG : GPIO_ENABLE1_REG) >> 
//                 (pin < 32 ? pin : pin - 32)) & 0x1)) {
//                 // Настраиваем пин как выход
//                 gpio_config_t io_conf = {
//                     .pin_bit_mask = (1ULL << pin),
//                     .mode = GPIO_MODE_OUTPUT,
//                     .pull_up_en = GPIO_PULLUP_DISABLE,
//                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
//                     .intr_type = GPIO_INTR_DISABLE
//                 };
//                 gpio_config(&io_conf);
//             }
            
//             write_gpio_reg(pin, value);
//         }

//         bool digitalRead() {
//             if (pin >= 40) return false;
            
//             // Сначала пытаемся прочитать напрямую
//             bool value = read_gpio_reg(pin);
            
//             // Если пин настроен как выход, временно переключаем его
//             // if ((REG_READ(pin < 32 ? GPIO_ENABLE_REG : GPIO_ENABLE1_REG) >> 
//             //     (pin < 32 ? pin : pin - 32)) & 0x1) {
//             //     return read_output_pin(pin);
//             // }
            
//             return value;
//         }
        
//         // void analogWrite(int state) {
//         //     if (pin >= 40) return;
            
//         //     if (!pwm_pins[pin].initialized) {
//         //         if (next_pwm_channel >= 16) return; // ESP32 имеет 16 каналов LEDC
                
//         //         // Настраиваем таймер (один на 8 каналов)
//         //         if (next_pwm_channel % 8 == 0) {
//         //             ledc_timer_config_t timer_cfg = {
//         //                 .speed_mode = LEDC_LOW_SPEED_MODE,
//         //                 .timer_num = next_pwm_channel,
//         //                 .duty_resolution = LEDC_TIMER_8_BIT,
//         //                 .freq_hz = 1000,
//         //                 .clk_cfg = LEDC_AUTO_CLK
//         //             };
//         //             ledc_timer_config(&timer_cfg);
//         //         }
                
//         //         // Настраиваем канал
//         //         ledc_channel_config_t channel_cfg = {
//         //             .speed_mode = LEDC_LOW_SPEED_MODE,
//         //             .channel = next_pwm_channel % 8,
//         //             .timer_sel = next_pwm_channel / 8,
//         //             .intr_type = LEDC_INTR_DISABLE,
//         //             .gpio_num = pin,
//         //             .duty = 0,
//         //             .hpoint = 0
//         //         };
//         //         ledc_channel_config(&channel_cfg);
                
//         //         pwm_pins[pin].initialized = true;
//         //         next_pwm_channel++;
//         //     }
            
//         //     // Устанавливаем скважность
//         //     pwm_pins[pin].duty = value;
//         //     uint32_t duty = (value * 255) / 255; // 8-bit resolution
            
//         //     ledc_set_duty(LEDC_LOW_SPEED_MODE, (next_pwm_channel - 1) % 8, duty);
//         //     ledc_update_duty(LEDC_LOW_SPEED_MODE, (next_pwm_channel - 1) % 8);
//         // }

//         // int analogRead() {
//         //     int8_t channel = get_adc_channel(pin);
//         //     if (channel < 0) return 0;
            
//         //     // Настраиваем ADC один раз
//         //     static bool adc_initialized = false;
//         //     if (!adc_initialized) {
//         //         adc1_config_width(ADC_WIDTH_BIT_12);
//         //         adc_initialized = true;
//         //     }
            
//         //     // Настраиваем канал и читаем
//         //     adc1_config_channel_atten(channel, ADC_ATTEN_DB_11);
//         //     int value = adc1_get_raw(channel);
            
//         //     return value >= 0 ? value : 0;
//         // }

//         int getMode() {
//             return mode;
//         }

//         void setMode(int mode) {
//             this->mode = mode;

//             gpio_config_t cfg = {0};
//             cfg.pin_bit_mask = (1ULL << pin);
//             cfg.intr_type = GPIO_INTR_DISABLE;
            
//             switch(mode) {
//                 case INPUT:
//                     cfg.mode = GPIO_MODE_INPUT;
//                     break;
//                 case OUTPUT:
//                     cfg.mode = GPIO_MODE_OUTPUT;
//                     break;
//                 case INPUT_PULLUP:
//                     cfg.mode = GPIO_MODE_INPUT;
//                     cfg.pull_up_en = GPIO_PULLUP_ENABLE;
//                     break;
//                 case INPUT_PULLDOWN:
//                     cfg.mode = GPIO_MODE_INPUT;
//                     cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
//                     break;
//                 case OUTPUT_OPEN_DRAIN:
//                     cfg.mode = GPIO_MODE_OUTPUT_OD;
//                     break;
//                 case ANALOG:
//                     cfg.mode = GPIO_MODE_INPUT;
//                     cfg.pull_up_en = GPIO_PULLUP_DISABLE;
//                     cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
//                     break;
//                 default:
//                     return;
//             }
            
//             gpio_config(&cfg);

//             if(mode == OUTPUT) {
//                 detachInterrupt(pin);
//             }
//             else {
//                 attachInterruptArg(pin, ISR, interruptTask, CHANGE);
//             }
//         }

//         int getPin() {
//             return pin;
//         }

//         void onInterrupt(int edge, std::function<void(Task *)> callback) {
//             if(edge == RISING) {
//                 this->handlersRising.push_back(onDemand(cpu_hal_get_core_id() == 0 ? CORE0 : CORE1, callback));
//             }
//             else {
//                 this->handlersFalling.push_back(onDemand(cpu_hal_get_core_id() == 0 ? CORE0 : CORE1, callback));
//             }

//             if(interruptTask == nullptr) {
//                 interruptTask = onDemand(CORE1, [this](Task *) {
//                     if(digitalRead() == HIGH) {
//                         for(int i=0; i < this->handlersRising.size(); i++) {
//                             handlersRising[i]->execute();
//                         }
//                     }
//                     else {
//                         for(int i=0; i < this->handlersFalling.size(); i++) {
//                             handlersFalling[i]->execute();
//                         }
//                     }
//                 });

                
//             }
//         }

//         void removeInterrupt(Task * task) {
//             handlersRising.erase(std::remove(handlersRising.begin(), handlersRising.end(), task), handlersRising.end());
//             handlersFalling.erase(std::remove(handlersFalling.begin(), handlersFalling.end(), task), handlersFalling.end());
//         }
//     };
// }