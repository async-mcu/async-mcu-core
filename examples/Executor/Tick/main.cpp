#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";


void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);

  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());
  
  onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 1 Deep 2000ms, time: %llu ms, core: %d", rts_ms(), xPortGetCoreID());

    int count = 0;
    uint64_t start_time = micros();
    while(true) {
      count++;

      if(count > 1000) {
        ESP_LOGI(TAG_MAIN, "for count > 1000, lambda time: %llu us, core: %d us", micros() - start_time, xPortGetCoreID());
        break;
      }
    }
    
    start_time = micros();

    onTick(CORE0, [count = 0, start_time](Task & that) mutable {
      count++;

      if(count > 1000) {
        ESP_LOGI(TAG_MAIN, "onTick count > 1000, lambda time: %llu us, core: %d us", micros() - start_time, xPortGetCoreID());
        that.cancel();
        delay(100);
      }
    });
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}