// Pin::digitalWrite / Pin::analogWrite — управление уровнями GPIO.
//
// pin13 (OUTPUT) цикл по 2 с: digitalWrite HIGH -> LOW -> analogWrite 256 (~25%
// шкалы 10 бит, ШИМ 5 кГц) -> analogWrite 768 (~75%).
// pin15 (OUTPUT) — меандр digitalWrite с периодом 4 с.
//
// До 2026-11-08 пример был копией старого interrupt-каркаса (issues.md #26) —
// переписан по назначению.
//
// Проверка на стенде (test/): пины соединены с каналами 4 и 5 стимулятора;
// команда DUTY превращает C3 в измеритель скважности (N замеров уровня на входе)
// и различает все четыре фазы pin13: ~100% / ~0% / ~25% / ~75%, и blink pin15.
//
// Особенность API, показанная здесь: после analogWrite пин остаётся подключённым
// к LEDC через GPIO-матрицу (режим OUTPUT_ANALOG). noTone() только останавливает
// ШИМ (пин замирает на idle-уровне LEDC), но НЕ перепривязывает матрицу —
// gpio_set_level после него игнорируется (проверено на стенде: 0% вместо HIGH).
// Возврат к digitalWrite — только через setMode(OUTPUT): он и глушит LEDC,
// и переключает матрицу обратно на GPIO.
#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

Pin pin13(13, OUTPUT);
Pin pin15(15, OUTPUT);

static int phase = 0;

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();

  onRepeat<Active>(Duration::ms(2000), [](Task &) {
    switch (phase) {
      case 0:
        pin13.setMode(OUTPUT);       // вернуть пин от LEDC к GPIO (см. шапку)
        pin13.digitalWrite(HIGH);
        ESP_LOGI(TAG_MAIN, "pin13: digitalWrite HIGH");
        break;
      case 1:
        pin13.setMode(OUTPUT);
        pin13.digitalWrite(LOW);
        ESP_LOGI(TAG_MAIN, "pin13: digitalWrite LOW");
        break;
      case 2:
        pin13.analogWrite(256);      // ~25% из 1023 (LEDC 10 bit)
        ESP_LOGI(TAG_MAIN, "pin13: analogWrite 256 (~25%%)");
        break;
      case 3:
        pin13.analogWrite(768);      // ~75%
        ESP_LOGI(TAG_MAIN, "pin13: analogWrite 768 (~75%%)");
        break;
    }
    pin15.digitalWrite((phase % 2) ? LOW : HIGH);
    phase = (phase + 1) % 4;
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
