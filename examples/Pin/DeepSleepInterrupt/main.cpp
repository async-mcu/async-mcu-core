#include <Arduino.h>
#include <async/Pin.h>

using namespace async;

static const char* TAG_MAIN = "MAIN";

//Pin pin13_RTC(13, INPUT_PULLUP);
Pin pin14_RTC(14, INPUT_PULLUP);
Pin pin19_RTC(19, INPUT_PULLDOWN);
//Pin pin33_RTC(33, INPUT_PULLUP);

void setup() {
  esp_log_level_set("*", ESP_LOG_ERROR);
  esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_TASK, ESP_LOG_VERBOSE);

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  // onRepeat<Deep>(Duration::ms(2500), CORE0, [](Task * task) {
  //   ESP_LOGI(TAG_MAIN, "2.5 seconds passed core %d, time %llu ms, mem %d, free stack space0: %d, free stack space1: %d, tasks0 %d, tasks1 %d",
  //     CURRENT_CORE, 
  //     rts_ms(), 
  //     heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
  //     uxTaskGetStackHighWaterMark(taskLoopCore0),
  //     uxTaskGetStackHighWaterMark(taskLoopCore1),
  //     tasks[0].size(),
  //     tasks[1].size());
  // });

  // onRepeat<Deep>(Duration::ms(10000), CORE1, [](Task * task) {
  //   ESP_LOGI(TAG_MAIN, "10 seconds passed core %d, time %llu ms, mem %d, free stack space0: %d, free stack space1: %d, tasks0 %d, tasks1 %d", 
  //     CURRENT_CORE, 
  //     rts_ms(), 
  //     heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
  //     uxTaskGetStackHighWaterMark(taskLoopCore0),
  //     uxTaskGetStackHighWaterMark(taskLoopCore1),
  //     tasks[0].size(),
  //     tasks[1].size());

  //   for(auto t : tasks[0]) {
  //       ESP_LOGI(TAG_MAIN, "  task0 mode %d type %d", t->getMode(), t->getType());
  //   } 
  //   for(auto t : tasks[1]) {
  //       ESP_LOGI(TAG_MAIN, "  task1 mode %d type %d", t->getMode(), t->getType());
  //   }
  // });
  
  // multi interrupts on pin14_RTC
  pin14_RTC.onInterrupt<Deep>(FALLING, []() {
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::FALLING, time %llu ms", rts_ms());

    pin19_RTC.onInterrupt<Light>(RISING, []() {
      ESP_LOGI(TAG_MAIN, "pin19_RTC onInterrupt::RISING, time %llu ms", rts_ms());
    });
  });

  // pin14_RTC.onInterrupt<Deep>(ONLOW, []() {
  //   ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::ONLOW, time %llu ms", rts_ms());
  // });

  pin14_RTC.onInterrupt<Deep>(RISING, []() {
    ESP_LOGI(TAG_MAIN, "pin14_RTC onInterrupt::RISING, time %llu ms", rts_ms());
  });

  start();
};

void loop() {
  vTaskDelete(NULL);
}