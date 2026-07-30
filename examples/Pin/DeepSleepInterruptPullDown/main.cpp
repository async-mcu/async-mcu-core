// Пробуждение из deep sleep по GPIO.
// Вариант PullDown: ножки в INPUT_PULLDOWN (прижаты к GND, в покое LOW).
// Пробуждение/прерывание — при подаче на ножку VCC/3.3V (фронт RISING),
// снятии обратно (FALLING).
// Противоположность примеру DeepSleepInterruptPullUp (INPUT_PULLUP).
#include <Async.h>

using namespace async;

Pin pin14_RTC(14, INPUT_PULLDOWN);
Pin pin27_RTC(27, INPUT_PULLDOWN);

static const char* TAG_MAIN = "MAIN";

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_INTERRUPT, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

  initAsync();
  
  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  pin14_RTC.addInterrupt<Deep>(FALLING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::FALLING, time %llu ms", rts_ms());
  });

  pin14_RTC.addInterrupt<Deep>(RISING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::RISING, time %llu ms", rts_ms());
  });

  pin27_RTC.addInterrupt<Deep>(FALLING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin27_RTC onInterrupt::FALLING, time %llu ms", rts_ms());
  });

  pin27_RTC.addInterrupt<Deep>(RISING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin27_RTC onInterrupt::RISING, time %llu ms", rts_ms());
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}