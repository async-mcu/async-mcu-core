#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  // Create two durations
  auto duration1 = Duration::ms(1500); // 1.5 seconds
  auto duration2 = Duration::ms(3000); // 3 seconds

  // Addition
  // ВАЖНО: add/subtract возвращают Duration (объект), а не число — в вариадик
  // ESP_LOGx можно передавать только значение (.ms()/.us()), иначе UB.
  auto sum = duration1->add(*duration2);
  ESP_LOGI(TAG_MAIN, "Sum: %llu ms", sum.ms());

  // Subtraction
  auto diff = duration2->subtract(*duration1);
  ESP_LOGI(TAG_MAIN, "Difference: %llu ms", diff.ms());

  // Check if 3 seconds have passed
  if (duration2->after(*duration1)) {
    ESP_LOGI(TAG_MAIN, "3 seconds have not yet passed.");
  } else {
    ESP_LOGI(TAG_MAIN, "3 seconds have passed.");
  }
  
  // Check if 3 seconds have passed
  if (duration1->before(*duration2)) {
    ESP_LOGI(TAG_MAIN, "1.5 seconds have not yet passed.");
  } else {
    ESP_LOGI(TAG_MAIN, "3 seconds have passed.");
  }
}

void loop() {
  auto from = Duration::now();

  vTaskDelay(pdMS_TO_TICKS(1000));

  ESP_LOGI(TAG_MAIN, "Difference random: %llu ms", Duration::now().subtract(from).ms());
}