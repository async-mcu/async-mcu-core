// Пробуждение из deep sleep по GPIO.
// Вариант PullUp: ножки в INPUT_PULLUP (прижаты к VCC, в покое HIGH).
// Пробуждение/прерывание — при подключении ножки к GND (фронт FALLING),
// отпускании обратно (RISING).
// Противоположность примеру DeepSleepInterruptPullDown (INPUT_PULLDOWN).
#include <Async.h>

using namespace async;

Pin pin26_RTC(26, INPUT_PULLUP);
Pin pin13_RTC(13, INPUT_PULLUP);

static const char* TAG_MAIN = "MAIN";

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_INTERRUPT, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  pin26_RTC.addInterrupt<Deep>(FALLING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin26_RTC onInterrupt::FALLING, time %llu ms", rts_ms());
  });

  pin26_RTC.addInterrupt<Deep>(RISING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin26_RTC onInterrupt::RISING, time %llu ms", rts_ms());
  });

  pin13_RTC.addInterrupt<Deep>(FALLING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin13_RTC onInterrupt::FALLING, time %llu ms", rts_ms());
  });

  pin13_RTC.addInterrupt<Deep>(RISING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "pin13_RTC onInterrupt::RISING, time %llu ms", rts_ms());
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
