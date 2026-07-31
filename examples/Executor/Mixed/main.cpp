#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

// Светодиод-индикатор на GPIO25: мигает в onDelay<Deep>/onRepeat<Deep> через onDelay<Active>.
// Pin::onInit (внутри initAsync) перенастраивает пин как выход на каждом boot — после
// DEEPSLEEP_RESET пины сбрасываются.
Pin led(25, OUTPUT, LOW);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  esp_log_level_set(TAG_EXECUTOR, ESP_LOG_INFO);

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
    // Мигание светодиодом на GPIO25: вкл сразу, выкл через onDelay<Active> 100 мс (без блокировки колбэка).
    led.digitalWrite(1);
    onDelay<Active>(Duration::ms(100), [](Task &) { led.digitalWrite(0); });

    onDelay<Light>(Duration::ms(700), CORE1, [](Task &) {
      ESP_LOGI(TAG_MAIN, "onDelay 2 Light 700ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);

      // enable wifi
      onDelay<Active>(Duration::ms(300), CORE1, [](Task &) {
        ESP_LOGI(TAG_MAIN, "onDelay 2 Active 300ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
      });
    });
  });

  // onRepeat<Light>(Duration::ms(3000), CORE1, [](Task &) {
  //   ESP_LOGI(TAG_MAIN, "onRepeat 3 Light 3000ms, time: %llu, core: %d", rts_ms(), CURRENT_CORE);
  // });

  beforeEnterSleep([](SleepMode) {
    vTaskDelay(pdMS_TO_TICKS(100)); // вынуть логи в UART до ухода в сон (CH340 теряет их при light/deep sleep)
  });

  afterWakeUp([](SleepMode m) {
    ESP_LOGI(TAG_MAIN, "afterWakeUp: %s, time: %llu ms", modeToStr(m), rts_ms());
    //vTaskDelay(pdMS_TO_TICKS(100)); // flush после пробуждения, до новой регистрации/сна
  });

  startAsync();
};

void loop() {
  vTaskDelete(NULL);
}