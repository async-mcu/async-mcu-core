#include <async/Logging.h>
#include <async/Duration.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  // Create two durations
  auto duration1 = Duration::ms(1500); // 1.5 seconds
  auto duration2 = Duration::ms(3000); // 3 seconds

  // Addition
  auto sum = duration1->add(*duration2);
  ESP_LOGI(TAG_MAIN, "Sum: %d seconds", sum);

  // Subtraction
  auto diff = duration2->subtract(*duration1);
  ESP_LOGI(TAG_MAIN, "Difference: %d seconds", diff);

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

  ESP_LOGI(TAG_MAIN, "Difference random: %d millis", Duration::now().subtract(from));
}