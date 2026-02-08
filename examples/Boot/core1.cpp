#include <async/Boot.h>

using namespace async;
static const char* TAG_CORE1 = "CORE1";

Boot boot1([]() {
  esp_log_level_set(TAG_CORE1, ESP_LOG_VERBOSE);

  onRepeat<Deep>(Duration::ms(2500), CORE1, [](Task *) {
    ESP_LOGI(TAG_CORE1, "Repeat task 1, every 2500 ms, time: %llu ms", rts_ms());
  });
});