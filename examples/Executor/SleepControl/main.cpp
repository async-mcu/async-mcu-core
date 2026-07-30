#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  setManualSleepMode(SleepMode::Deep);

  esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO);

  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    if(getManualSleepMode() == SleepMode::Deep || getManualSleepMode() == SleepMode::None) {
      setManualSleepMode(SleepMode::Light);
      ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Light");
    }
    else if(getManualSleepMode() == SleepMode::Light) {
      setManualSleepMode(SleepMode::Active);
      ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Active");
    } 
    else if(getManualSleepMode() == SleepMode::Active) {
      setManualSleepMode(SleepMode::Deep);
      ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Deep");
    }

    ESP_LOGI(TAG_MAIN, "onRepeat 1 Active 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  beforeEnterSleep([](SleepMode sleepMode) {
    ESP_LOGI(TAG_MAIN, "Before enter to sleep mode: %s", modeToStr(sleepMode));
    vTaskDelay(pdMS_TO_TICKS(100)); // wait fo flush logs before sleep
  });

  afterWakeUp([](SleepMode sleepMode) {
    ESP_LOGI(TAG_MAIN, "After wake up from sleep mode: %s", modeToStr(sleepMode));
    vTaskDelay(pdMS_TO_TICKS(100)); // wait fo flush logs after wake up
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}