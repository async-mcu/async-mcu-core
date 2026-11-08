// PinProbe — диагностика проводки DUT↔стимулятор: логирует уровни пинов
// каждые 100 мс (без прерываний). Волна/WAVE на канале стимулятора должна
// давать чередование 0/1 в логе соответствующего пина.
#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

Pin p14(14, INPUT_PULLUP);
Pin p19(19, INPUT_PULLDOWN);
Pin p26(26, INPUT_PULLUP);
Pin p13(13, INPUT_PULLDOWN);
Pin p15(15, INPUT_PULLDOWN);
Pin p27(27, INPUT_PULLDOWN);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);
  initAsync();

  onRepeat<Active>(Duration::ms(100), [](Task &) {
    ESP_LOGI(TAG_MAIN, "lvls 14=%d 19=%d 26=%d 13=%d 15=%d 27=%d",
             p14.digitalRead(), p19.digitalRead(), p26.digitalRead(),
             p13.digitalRead(), p15.digitalRead(), p27.digitalRead());
  });

  startAsync();
}

void loop() { vTaskDelete(NULL); }
