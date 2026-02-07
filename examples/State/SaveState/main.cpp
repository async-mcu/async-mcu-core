#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

RTC_DATA_ATTR State<int> counter(0);

void setup() {
  init();

  counter.onChange([](int prev, int current){ 
    ESP_LOGI(TAG_MAIN, "onChange count of boots: prev %d, current %d \n", prev, current); 
  });

  ESP_LOGI(TAG_MAIN, "Count of boots %d \n", counter.set([](int prev){ 
    return prev + 1; 
  }));

  onDelay<Active>(Duration::ms(100), [](Task *) {
    esp_sleep_enable_timer_wakeup(1000 * 1000);
    esp_deep_sleep_start(); 
  });

  start();
}

void loop() {
  vTaskDelete(NULL);
}