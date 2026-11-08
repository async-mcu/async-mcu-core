// Pin::digitalRead — чтение уровней GPIO опросом (без прерываний).
//
// Два пина (13, 15) опрашиваются задачей каждые 50 мс; логируется только ИЗМЕНЕНИЕ
// уровня (программная детекция фронтов) с таймстампом. До 2026-11-08 пример был
// копией старого interrupt-каркаса (issues.md #26) — переписан по назначению.
//
// Проверка на стенде (auxiliary/): пины соединены с каналами 4 и 5 стимулятора
// (C3 GPIO6/GPIO7); команда SET меняет уровень линии — пример обязан увидеть
// переход с задержкой не больше периода опроса (~50 мс).
//
// analogRead здесь не демонстрируется: АЦП — только ADC1 (GPIO32-39), на стенде
// эти пины не разведены.
#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

Pin pin13(13, INPUT_PULLDOWN);
Pin pin15(15, INPUT_PULLDOWN);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();

  int lvl13 = pin13.digitalRead();
  int lvl15 = pin15.digitalRead();
  ESP_LOGI(TAG_MAIN, "start: pin13=%d pin15=%d", lvl13, lvl15);

  // Опрос каждые 50 мс; состояние уровней живёт в самой лямбде (mutable) —
  // логируем только переходы.
  onRepeat<Active>(Duration::ms(50), [lvl13, lvl15](Task &) mutable {
    int a = pin13.digitalRead();
    int b = pin15.digitalRead();
    if (a != lvl13) {
      ESP_LOGI(TAG_MAIN, "pin13: %d -> %d (t=%llu ms)", lvl13, a, rts_ms());
      lvl13 = a;
    }
    if (b != lvl15) {
      ESP_LOGI(TAG_MAIN, "pin15: %d -> %d (t=%llu ms)", lvl15, b, rts_ms());
      lvl15 = b;
    }
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
