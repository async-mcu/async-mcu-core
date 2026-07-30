#include <algorithm>
#include <async/Pin.h>
#include <async/Executor.h>
#include <async/Definitions.h>
#include "hal/gpio_ll.h"

using namespace async;

// Static members initialization
adc_oneshot_unit_handle_t Pin::adc1Handle = nullptr;
bool Pin::adcUnitInit = false;
bool isrServiceInstalled = false;

QueueHandle_t Pin::irqQueue = nullptr;
TaskHandle_t Pin::irqTask = nullptr;
const int validDeepSleepPins[] = {0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39};

bool isValidRtcPin(int pin) {
    for(int i = 0; i < sizeof(validDeepSleepPins)/sizeof(validDeepSleepPins[0]); i++) {
        if(pin == validDeepSleepPins[i]) return true;
    }
    return false;
}

// Private helper implementations
void Pin::ensureIrqDispatch() {
    if (irqQueue == nullptr) {
        irqQueue = xQueueCreate(32, sizeof(Pin *));
        xTaskCreatePinnedToCore(Pin::irqDispatchTask, "pinIrq", 4096, nullptr, 1, &irqTask, 0);
    }
}

void Pin::irqDispatchTask(void *) {
    Pin * pin = nullptr;
    while (xQueueReceive(irqQueue, &pin, portMAX_DELAY) == pdTRUE) {
        if (pin) {
            pin->interruptTask->schedule();
        }
    }
}

void IRAM_ATTR Pin::interrupt() {
    int value = digitalRead(false);

    if(!interrupted) {
        interrupted = true;
        interruptTask->setValue((void *) value);

        Pin * self = this;
        if (xPortInIsrContext()) {
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            if (xQueueSendFromISR(irqQueue, &self, &higherPriorityTaskWoken) != pdTRUE) {
                interrupted = false; // очередь переполнена — повтор на следующем фронте
            }
            if (higherPriorityTaskWoken == pdTRUE) {
                portYIELD_FROM_ISR();
            }
        } else {
            if (xQueueSend(irqQueue, &self, 0) != pdTRUE) {
                interrupted = false;
            }
        }
    }
}

void Pin::stopLedcInternal() {
    if (isLedcActive) {
        ledc_stop(LEDC_LOW_SPEED_MODE, ledChan, 0);
        isLedcActive = false;
    }
}

adc_channel_t Pin::getAdcChannelInternal() {
    switch (pinNum) {
        case 36: return ADC_CHANNEL_0; case 37: return ADC_CHANNEL_1;
        case 38: return ADC_CHANNEL_2; case 39: return ADC_CHANNEL_3;
        case 32: return ADC_CHANNEL_4; case 33: return ADC_CHANNEL_5;
        case 34: return ADC_CHANNEL_6; case 35: return ADC_CHANNEL_7;
        default: return (adc_channel_t)-1;
    }
}

// Constructor
Pin::Pin(int pin, int mode, int defaultLevel)
    : pinNum((gpio_num_t)pin), currentMode(mode), level(defaultLevel) {
    interruptTask = onDemand([this](Task & demandTask) {
        if(!isStarted()) return; // wait start

        int value = (int) demandTask.getValue();
        ESP_LOGV(TAG_PIN, "onDemand pin %d", pinNum);

        if(this->sleepMode == SleepMode::Deep || this->sleepMode == SleepMode::Light) {
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

        for(auto interruptParam : interruptParams) {
            if(interruptParam->type == ONLOW && value == LOW) {
                onTick([interruptParam, this](Task & tickTask) {
                    int value = digitalRead();

                    if(value == HIGH) {
                        tickTask.cancel();
                    }
                    else {
                        interruptParam->callback(*interruptParam);
                    }
                });
            }
            else if(interruptParam->type == ONHIGH && value == HIGH) {
                onTick([interruptParam, this](Task & tickTask) {
                    int value = digitalRead();

                    if(value == LOW) {
                        tickTask.cancel();
                    }
                    else {
                        interruptParam->callback(*interruptParam);
                    }
                });
            }
            else if(interruptParam->type == RISING && value == HIGH) {
                interruptParam->callback(*interruptParam);
            }
            else if(interruptParam->type == FALLING && value == LOW) {
                interruptParam->callback(*interruptParam);
            }
            else if(interruptParam->type == CHANGE) {
                interruptParam->callback(*interruptParam);
            }
        }

        interrupted = false;
    });

    onInit(CURRENT_CORE, [this](Task &) {
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
}

// Mode configuration
void Pin::setMode(uint8_t mode) {
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

bool Pin::isReverted() {
    return revert;
}

Interrupt * Pin::addInterrupt(SleepMode sleepMode, gpio_int_type_t type, std::function<void(Interrupt &)> callback) {
        // В Deep-режиме будить можно только по фронту RISING/FALLING:
        // механизм ext0/ext1 + revert работает с уровнями, имитируя фронты.
        if (sleepMode == SleepMode::Deep && type != RISING && type != FALLING) {
            ESP_LOGE(TAG_PIN, "Deep interrupt on pin %d supports only RISING/FALLING (got type %d)", pinNum, (int) type);
            esp_system_abort("Deep sleep interrupts support only RISING and FALLING edges");
        }

        ensureIrqDispatch();

        auto interruptParam = new Interrupt(*this, type, sleepMode, callback);

        // Update sleepMode with priority: Active > Light > Deep
        if (sleepMode == SleepMode::Active) {
            this->sleepMode = SleepMode::Active;
        } else if (sleepMode == SleepMode::Light && this->sleepMode != SleepMode::Active) {
            this->sleepMode = SleepMode::Light;
        } else if (sleepMode == SleepMode::Deep && this->sleepMode == SleepMode::None) {
            this->sleepMode = SleepMode::Deep;
        }

        // add callback to list
        interruptParams.push_back(interruptParam);
        addGlobalInterruptParam(interruptParam);

        // first install isr service
        if (!isrServiceInstalled) {
            gpio_install_isr_service(0);
            isrServiceInstalled = true;
        }

        // interrupt task already initialized in constructor
        if(interruptParam->sleepMode == SleepMode::Deep || interruptParam->sleepMode == SleepMode::Light) {
            // Configure wakeup settings for Deep/Light sleep
        }

        bool rising = false;
        bool falling = false;
        bool onlow = false;
        bool onhigh = false;
        bool anyedge = false;

        //
        for(auto interruptParam : interruptParams) {
            if(interruptParam->type == RISING) rising = true;
            else if(interruptParam->type == FALLING) falling = true;
            else if(interruptParam->type == ONLOW) onlow = true;
            else if(interruptParam->type == ONHIGH) onhigh = true;
            else if(interruptParam->type == CHANGE) anyedge = true;
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
        if(this->sleepMode == SleepMode::Active) {
            setInterruptSleepMode(SleepMode::Active);
        }
        else if(this->sleepMode == SleepMode::Light) {
            setInterruptSleepMode(SleepMode::Light);
            ESP_LOGD(TAG_PIN, "gpio_wakeup_enable pin %d to %d", pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable(pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
        }
        else if(this->sleepMode == SleepMode::Deep) {
            setInterruptSleepMode(SleepMode::Deep);

            ESP_LOGD(TAG_PIN, "gpio_wakeup_enable from deep pin %d to %d", pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
            gpio_wakeup_enable(pinNum, (currentMode == INPUT_PULLDOWN) ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);

            if(!isValidRtcPin(pinNum)) {
                esp_system_abort("Pin is not RTC!");
            }

            for(Interrupt * interruptParam : getGlobalInterruptParams()) {
                if(interruptParam->sleepMode == SleepMode::Deep && interruptParam->pin.getMode() != currentMode) {
                    ESP_LOGE(TAG_PIN, "Deep wake pins must share one pull mode: pin %d mode %d != %d", interruptParam->pin.getPin(), interruptParam->pin.getMode(), currentMode);
                    esp_system_abort("Deep sleep: all wake pins must use the same pull (INPUT_PULLUP or INPUT_PULLDOWN)");
                }
            }

            setDeepInterruptsMode(currentMode);
                    
            esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
            
            if (cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) {
                ESP_LOGD(TAG_PIN, "Wakeup from deep on pin %d, ext0 %d, digitalRead %d, wakeup: %d", pinNum, cause == ESP_SLEEP_WAKEUP_EXT0, digitalRead(), (esp_sleep_get_ext1_wakeup_status() & (1ULL << pinNum)));

                bool shouldInterrupt = (cause == ESP_SLEEP_WAKEUP_EXT0)
                    || (currentMode == INPUT_PULLUP) 
                    || (digitalRead() == !getDeepInterruptsRevert() ? (currentMode == INPUT_PULLDOWN ? HIGH : LOW) : (currentMode != INPUT_PULLDOWN ? HIGH : LOW)) 
                    || (esp_sleep_get_ext1_wakeup_status() & (1ULL << pinNum));
                
                if (shouldInterrupt) {
                    this->interrupt();
                }
            }
        }

        return interruptParam;
}

// Interrupt management
void Pin::removeInterrupt(Interrupt * interrupt) {
    ESP_LOGD(TAG_PIN, "Remove interrupt pin %d", pinNum);

    interruptParams.erase(std::remove(interruptParams.begin(), interruptParams.end(), interrupt), interruptParams.end());
    removeGlobalInterruptParam(interrupt);

    bool active = false;
    bool light = false;
    bool deep = false;

    for(auto interruptParam : getGlobalInterruptParams()) {
        if(interruptParam->pin.getPin() == pinNum) {
            if(interruptParam->sleepMode == SleepMode::Active) active = true;
            else if(interruptParam->sleepMode == SleepMode::Light) light = true;
            else if(interruptParam->sleepMode == SleepMode::Deep) deep = true;
        }
    }

    if(active) {
        setInterruptSleepMode(SleepMode::Active);
    }
    else if(light) {
        setInterruptSleepMode(SleepMode::Light);
    }
    else if(deep) {
        setInterruptSleepMode(SleepMode::Deep);
    }

    if(interruptParams.size() == 0) {
        ESP_LOGD(TAG_PIN, "gpio_intr_disable pin %d!", pinNum);
        gpio_intr_disable(getPin());
        gpio_wakeup_disable(getPin());
        setInterruptSleepMode(SleepMode::None);
    }
}

// ISR handler. IRAM: всё вызываемое отсюда обязано быть в IRAM/inline-HAL —
// высокоуровневые gpio_intr_disable/gpio_get_level лежат во flash в этом IDF.
void IRAM_ATTR Pin::ISR(void* arg) {
    Pin *instance = (Pin*) arg;
    gpio_ll_intr_disable(&GPIO, instance->getPin()); // register-only, inline → IRAM-safe
    ESP_DRAM_LOGV(TAG_PIN, "ISR pin %d!\n", instance->getPin());
    instance->interrupt();
}

gpio_num_t IRAM_ATTR Pin::getPin() {
   return pinNum;
}

uint8_t Pin::getMode() {
    return currentMode;
}

void Pin::digitalWrite(int level, bool disableModeCheck) {
    if (!disableModeCheck && !(currentMode & 0x02)) setMode(OUTPUT);
    gpio_set_level(pinNum, level);
    this->level = level;
}

// setAutoMode=false — безопасный для ISR путь: пропускает setMode(INPUT)
// (тяжёлая flash-логика). Уровень читаем через gpio_ll_get_level (inline-HAL →
// инлайнится в IRAM); высокоуровневый gpio_get_level в этом IDF лежит во flash.
int IRAM_ATTR Pin::digitalRead(bool setAutoMode) {
    if (setAutoMode && currentMode == ANALOG) setMode(INPUT);
    return gpio_ll_get_level(&GPIO, pinNum);
}

int Pin::analogRead() {
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

    void Pin::tone(uint32_t freq, Duration * duration) {
        if (freq == 0) { stopLedcInternal(); return; }
        analogWrite(512); 
        ledc_set_freq(LEDC_LOW_SPEED_MODE, timerNum, freq);

        if(duration != nullptr) {
            onDelay<Active>(duration, [this](Task &) {
                stopLedcInternal();
            });
        }
    }

    void Pin::noTone() {
        stopLedcInternal();
    }

    
void Pin::analogWrite(int duty, bool disableModeCheck) {
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
