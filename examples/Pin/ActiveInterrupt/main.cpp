#include <Async.h>

using namespace async;

Pin pin14_RTC(14, INPUT_PULLUP);
Pin pin19_RTC(19, INPUT_PULLDOWN);

int onLowCount = 0;
static const char* TAG_MAIN = "MAIN";
Interrupt * pin19RisingInterrupt = nullptr;

void setup() {
  startAsync();

  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_INTERRUPT, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  pin14_RTC.addInterrupt<Active>(FALLING, [](Interrupt *) {
    onLowCount = 0;
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::FALLING, time %llu ms", rts_ms());

    pin19RisingInterrupt = pin19_RTC.addInterrupt<Active>(RISING, [](Interrupt *) {
      ESP_LOGI(TAG_MAIN, "pin19_RTC onInterrupt::RISING, onLowCount %d, time %llu ms", onLowCount, rts_ms());
    });
  });

  // multi interrupts on pin14_RTC
  pin14_RTC.addInterrupt<Active>(ONLOW, [](Interrupt *) {
    onLowCount++;
    //ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::ONLOW, time %llu ms\n", rts_ms());
  });

  pin14_RTC.addInterrupt<Active>(RISING, [](Interrupt *) {
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::RISING, time %llu ms", rts_ms());

    if(pin19RisingInterrupt != nullptr) {
      pin19RisingInterrupt->cancel();
    }
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}