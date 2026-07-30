#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "ACTIVE_CANCEL";

// Тест фикса п.1: утечка activeTasksCount при отмене active-задачи.
//
// Сценарий:
//   1. Light-задача (1 c) — повод для light sleep.
//   2. Active-задача (500 мс) — держит CPU активным (activeTasksCount = 1).
//   3. Через 3 c отменяем active. Корректный cancel() декрементирует
//      activeTasksCount до 0 → планировщик начинает уходить в sleep.
//
// Ожидаемый вывод (с фиксом):
//   ... ACTIVE tick / LIGHT tick ... (сна НЕТ — activeTasksCount > 0)
//   >>> cancel ACTIVE @ 3000 ms
//   ### SLEEP (Light) @ ...          ← вот это доказывает, что счетчик обнулился
//   LIGHT tick ...                   ← ACTIVE больше НЕ тикает
//
// Без фикса: после "cancel" строки "### SLEEP" НЕТ — счётчик утечь, CPU не спит.
void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO); // чтобы видеть индикатор сна

  initAsync();

  // Light-задача: каждые 1 c.
  onRepeat<Light>(Duration::ms(1000), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "LIGHT tick @ %llu ms", rts_ms());
  });

  // Active-задача: каждые 500 мс. Держит activeTasksCount = 1.
  Task *active = onRepeat<Active>(Duration::ms(500), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "ACTIVE tick @ %llu ms", rts_ms());
  });

  // Через 3 c отменяем active-задачу.
  onDelay<Active>(Duration::ms(3000), CORE0, [active](Task &) {
    ESP_LOGI(TAG_MAIN, ">>> cancel ACTIVE @ %llu ms", rts_ms());
    active->cancel();
  });

  // Индикатор сна библиотеки: сработает только когда MCU реально засыпает.
  beforeEnterSleep([](SleepMode m) {
    ESP_LOGI(TAG_MAIN, "### SLEEP (%s) @ %llu ms", modeToStr(m), rts_ms());
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
