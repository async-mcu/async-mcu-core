#include <async/Boot.h>

using namespace async;
static const char* TAG_CORE0 = "CORE0";

Boot boot0([]() {
  esp_log_level_set(TAG_CORE0, ESP_LOG_VERBOSE);

  onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task *) {
    ESP_LOGI(TAG_CORE0, "Repeat task 0, every 2000 ms, time: %llu ms", rts_ms());
  });
});