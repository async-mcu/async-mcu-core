// State в трёх sleep-режимах: deep, light, active.
//
// Колбэк onChange фиксирован на Active (State::set → onOnce всегда Active-задача,
// см. Executor.cpp:237), поэтому «режим set» здесь задаётся вручную через
// setSleepMode(). onRepeat<Deep> каждые 3 с циклически меняет режим
// Deep → Light → Active → Deep и инкрементит value; onChange логирует изменение.
//
// beforeEnterSleep / afterWakeUp показывают РЕАЛЬНЫЙ сон каждого режима:
//   • Deep   — esp_deep_sleep → DEEPSLEEP_RESET при каждом пробуждении
//              (value в RTC_DATA_ATTR переживает сброс и накапливается);
//   • Light  — esp_light_sleep, без сброса чипа;
//   • Active — сна нет, колбэк выполняется тут же (beforeEnterSleep НЕ зовётся).
//
// Почему режимы переключаются по одному, а не тремя одновременными onRepeat:
// наличие Active/Tick-задачи удерживает чип активным (activeTasksCount,
// Executor.cpp:399) — и реального deep/light сна не получилось бы.

#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

// RTC_DATA_ATTR — value переживает DEEPSLEEP_RESET (Deep-фаза сбрасывает чип).
RTC_DATA_ATTR State<int> value(0);

void setup() {
  setSleepMode(SleepMode::Deep);  // стартовый режим цикла (Deep → Light → Active → Deep)

  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO);  // видеть esp_*_sleep_start

  initAsync();

  value.onChange([](int prev, int current) {
    ESP_LOGI(TAG_MAIN, "onChange: %d -> %d", prev, current);
  });

  // Каждые 3 с: переключаем режим по циклу и инкрементим value.
  onRepeat<Deep>(Duration::ms(3000), [](Task &) {
    SleepMode next;
    switch (getSleepMode()) {
      case Deep:
      case None:  next = Light;  break;
      case Light: next = Active; break;
      default:    next = Deep;   break;  // Active
    }
    setSleepMode(next);
    ESP_LOGI(TAG_MAIN, "setSleepMode -> %s", modeToStr(next));
    value.set([](int v) { return v + 1; });
  });

  beforeEnterSleep([](SleepMode m) {
    ESP_LOGI(TAG_MAIN, "beforeEnterSleep: %s", modeToStr(m));
    vTaskDelay(pdMS_TO_TICKS(100)); // вынуть логи в UART до ухода в сон (иначе CH340 теряет их при light sleep)
  });
  afterWakeUp([](SleepMode m) {
    ESP_LOGI(TAG_MAIN, "afterWakeUp: %s", modeToStr(m));
  });

  ESP_LOGI(TAG_MAIN, "SleepModes running: Deep -> Light -> Active cycle, 3 s/step");
  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
