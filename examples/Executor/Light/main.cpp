#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  onDelay<Light>(Duration::ms(2000), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 1 Light 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  onDelay<Light>(Duration::ms(2000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 2 Light 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });
  
  onRepeat<Light>(Duration::ms(2000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onRepeat 2 Light 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}