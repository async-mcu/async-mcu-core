#include <algorithm>
#include <async/Pin.h>
#include <async/Executor.h>
#include <async/Definitions.h>

using namespace async;

// Static members initialization
adc_oneshot_unit_handle_t Pin::adc1Handle = nullptr;
bool Pin::adcUnitInit = false;
bool isrServiceInstalled = false;
const int validDeepSleepPins[] = {0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,37,38,39};

bool isValidRtcPin(int pin) {
    for(int i = 0; i < sizeof(validDeepSleepPins)/sizeof(validDeepSleepPins[0]); i++) {
        if(pin == validDeepSleepPins[i]) return true;
    }
    return false;
}

// Private helper implementations
void Pin::interrupt() {
    int value = digitalRead();

    if(!interrupted) {
        interrupted = true;
        interruptTask->setValue((void *) value);
        interruptTask->schedule();
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
    onInit(CURRENT_CORE, [this](Task *) {
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

Interrupt * Pin::addInterrupt(Mode mode, gpio_int_type_t type, std::function<void(Interrupt *)> callback) {

        auto interruptParam = new Interrupt(this, type, mode, callback);

        // add callback to list
        interruptParams.push_back(interruptParam);
        getGlobalInterruptParams().push_back(interruptParam);

        // first install isr service
        if (!isrServiceInstalled) {
            gpio_install_isr_service(0);
            isrServiceInstalled = true;
        }

        // first init interrupt task for this pin
        if(interruptTask == nullptr) {
            interruptTask = onDemand([mode, this](Task * demandTask) {
                if(!isStarted()) return; // wait start

                int value = (int) demandTask->getValue();
                ESP_LOGV(TAG_PIN, "onDemand pin %d", pinNum);

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

                for(auto interruptParam : interruptParams) {
                    if(interruptParam->type == ONLOW && value == LOW) {
                        onTick([interruptParam, this](Task * tickTask) {
                            int value = digitalRead();

                            if(value == HIGH) {
                                tickTask->cancel();
                            }
                            else {
                                interruptParam->callback(interruptParam);
                            }
                        });
                    }
                    else if(interruptParam->type == ONHIGH && value == HIGH) {
                        onTick([interruptParam, this](Task * tickTask) {
                            int value = digitalRead();

                            if(value == LOW) {
                                tickTask->cancel();
                            }
                            else {
                                interruptParam->callback(interruptParam);
                            }
                        });
                    }
                    else if(interruptParam->type == RISING && value == HIGH) {
                        interruptParam->callback(interruptParam);
                    }
                    else if(interruptParam->type == FALLING && value == LOW) {
                        interruptParam->callback(interruptParam);
                    }
                    else if(interruptParam->type == CHANGE) {
                        interruptParam->callback(interruptParam);
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

            for(Interrupt * interruptParam : getGlobalInterruptParams()) {
                if(interruptParam->pin->getMode() != currentMode && interruptParam->sleepMode == Mode::Deep) {
                    esp_system_abort("In deep sleep mode, modes for all pins must be the same (INPUT_PULLUP or INPUT_PULLDOWN)");
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
void Pin::removeInterrupt(Interrupt * task) {
    ESP_LOGD(TAG_PIN, "Remove interrupt pin %d", pinNum);

    interruptParams.erase(std::remove(interruptParams.begin(), interruptParams.end(), task), interruptParams.end());
    getGlobalInterruptParams().erase(std::remove(getGlobalInterruptParams().begin(), getGlobalInterruptParams().end(), task), getGlobalInterruptParams().end());

    bool active = false;
    bool light = false;
    bool deep = false;

    for(auto interruptParam : getGlobalInterruptParams()) {
        if(interruptParam->pinNum == pinNum) {
            if(interruptParam->sleepMode == Mode::Active) active = true;
            else if(interruptParam->sleepMode == Mode::Light) light = true;
            else if(interruptParam->sleepMode == Mode::Deep) deep = true;
        }
    }

    if(active) {
        setInterruptLevel(Mode::Active);
    }
    else if(light) {
        setInterruptLevel(Mode::Light);
    }
    else if(deep) {
        setInterruptLevel(Mode::Deep);
    }

    if(interruptParams.size() == 0) {
        ESP_LOGD(TAG_PIN, "gpio_intr_disable pin %d!", pinNum);
        gpio_intr_disable(getPin());
        gpio_wakeup_disable(getPin());
        setInterruptLevel(Mode::None);
    }
}

// ISR handler
void Pin::ISR(void* arg) {
    Pin *instance = (Pin*) arg;
    gpio_intr_disable(instance->getPin());
    ets_printf("ISR pin %d!\n", instance->getPin());
    instance->interrupt();
}

gpio_num_t Pin::getPin() {
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

int Pin::digitalRead() {
    if (currentMode == ANALOG) setMode(INPUT);
    return gpio_get_level(pinNum);
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
            onDelay<Active>(duration, [this](Task *) {
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
