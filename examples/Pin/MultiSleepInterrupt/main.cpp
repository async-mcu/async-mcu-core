#include <Async.h>

using namespace async;

Pin pin13_RTC(13, INPUT_PULLDOWN);
Pin pin15_RTC(15, INPUT_PULLDOWN);

static const char* TAG_MAIN = "MAIN";

Interrupt * interrupt15_Light = nullptr;

void setup() {
  initAsync();

  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  //esp_log_level_set(TAG_INTERRUPT, ESP_LOG_VERBOSE);
  //esp_log_level_set(TAG_PIN, ESP_LOG_DEBUG);
  //esp_log_level_set(TAG_EXECUTOR, ESP_LOG_DEBUG);

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  pin13_RTC.addInterrupt<Deep>(RISING, [](Interrupt *) {
    ESP_LOGI(TAG_MAIN, "pin13_RTC onInterrupt::RISING, time %llu ms", rts_ms());

    interrupt15_Light = pin15_RTC.addInterrupt<Light>(RISING, [](Interrupt *) {
      ESP_LOGI(TAG_MAIN, "pin15_RTC onInterrupt::RISING, time %llu ms", rts_ms());
    });
  });

  pin13_RTC.addInterrupt<Deep>(FALLING, [](Interrupt *) {
    ESP_LOGI(TAG_MAIN, "pin13_RTC onInterrupt::FALLING, time %llu ms", rts_ms());

    if(interrupt15_Light) {
      interrupt15_Light->cancel();
      interrupt15_Light = nullptr;
    }
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}