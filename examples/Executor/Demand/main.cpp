#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  auto demand = onDemand(CORE1, [](Task *) {
    ESP_LOGI(TAG_MAIN, "onDemand, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  onDelay<Deep>(Duration::ms(2000), CORE0, [&demand](Task *) {
    ESP_LOGI(TAG_MAIN, "onDelay 1 Deep 2000ms, time: %llu, core: %d",  rts_ms(), CURRENT_CORE);
    demand->execute();
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}