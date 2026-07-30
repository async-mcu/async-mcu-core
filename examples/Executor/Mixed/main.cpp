#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
  ESP_LOGI(TAG_MAIN, "Before script time %llu ms", rts_ms());

  initAsync();

  onDelay<Deep>(Duration::ms(2000), CORE0, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onDelay 1 Deep 2000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);

    onDelay<Light>(Duration::ms(1300), CORE1, [](Task &) {
      ESP_LOGI(TAG_MAIN, "onDelay 1 Light 1300ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);

      // enable wifi
      onDelay<Active>(Duration::ms(500), CORE1, [](Task &) {
        ESP_LOGI(TAG_MAIN, "onDelay 1 Active 500ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
      });
    });
  });

  onRepeat<Deep>(Duration::ms(5000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onRepeat 2 Deep 5000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);

    onDelay<Light>(Duration::ms(700), CORE1, [](Task &) {
      ESP_LOGI(TAG_MAIN, "onDelay 2 Light 700ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);

      // enable wifi
      onDelay<Active>(Duration::ms(300), CORE1, [](Task &) {
        ESP_LOGI(TAG_MAIN, "onDelay 2 Active 300ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
      });
    });
  });

  onRepeat<Light>(Duration::ms(3000), CORE1, [](Task &) {
    ESP_LOGI(TAG_MAIN, "onRepeat 2 Light 3000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}