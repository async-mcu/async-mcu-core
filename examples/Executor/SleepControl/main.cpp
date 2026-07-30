// Ручное управление сном (manual sleep control).
// В отличие от АВТОМАТИЧЕСКОГО выбора сна по типам задач (см. Executor/Mixed), здесь
// приложение САМО решает, в какой режим уходить: setSleepMode() задаёт
// максимально допустимый уровень, и каждые 2 с мы циклически меняем его сами —
// Deep → Light → Active → Deep. switch по текущему режиму (getSleepMode)
// выбирает следующий; колбэки beforeEnterSleep / afterWakeUp логируют вход в сон
// и пробуждение.

#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  setSleepMode(SleepMode::Deep);

  esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

  initAsync();

  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  // Каждые 2 с циклически переключаем ручной режим сна: Deep → Light → Active → Deep.
  // switch по текущему режиму решает, какой будет следующим.
  onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    SleepMode mode = getSleepMode();
    switch (mode) {
      case Deep:
      case None:
        setSleepMode(SleepMode::Light);
        ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Light");
        break;
      case Light:
        setSleepMode(SleepMode::Active);
        ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Active");
        break;
      case Active:
        setSleepMode(SleepMode::Deep);
        ESP_LOGI(TAG_MAIN, "Set manual sleep mode to Deep");
        break;
    }

    ESP_LOGI(TAG_MAIN, "onRepeat 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  beforeEnterSleep([](SleepMode sleepMode) {
    ESP_LOGI(TAG_MAIN, "Before enter to sleep mode: %s, time: %llu ms", modeToStr(sleepMode), rts_ms());
    vTaskDelay(pdMS_TO_TICKS(100)); // wait fo flush logs before sleep
  });

  afterWakeUp([](SleepMode sleepMode) {
    ESP_LOGI(TAG_MAIN, "After wake up from sleep mode: %s, time: %llu ms", modeToStr(sleepMode), rts_ms());
    vTaskDelay(pdMS_TO_TICKS(100)); // wait fo flush logs after wake up
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}