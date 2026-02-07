#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

State<int> value(0);

void setup() {
  value.onChange([](int prev, int current) { 
    ESP_LOGI(TAG_MAIN, "onChange 1 value: prev %d, current %d \n", prev, current); 
  });

  value.onChange([](int prev, int current) { 
    ESP_LOGI(TAG_MAIN, "onChange 2 value: prev %d, current %d \n", prev, current); 
  });

  ESP_LOGI(TAG_MAIN, "value %d \n", value.set([](int prev) { 
    return prev + 1; 
  }));
}

void loop() {
  vTaskDelete(NULL);
}