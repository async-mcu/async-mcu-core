#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
  
  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  onDelay<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 1 Deep 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  onDelay<Deep>(Duration::ms(2000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 2 Deep 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });
  
  onRepeat<Deep>(Duration::ms(2000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onRepeat 2 Deep 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}