// Pin::tone/noTone — генерация тона (меандр ~50%) на GPIO13 через LEDC.
//
// Две формы:
//   tone(freq, duration) — прозвенит duration и сам замолкнет (одноразовая
//     Active-задача остановки; до фикса #9 2026-11-09 каждая пара утекала —
//     теперь задача освобождается после срабатывания);
//   tone(freq) + noTone() — ручной старт/стоп, без утечек.
//
// Проверка на стенде (auxiliary/): GPIO13 соединён с каналом 4 (C3 GPIO6);
// команда FREQ превращает C3 в частотомер и меряет реальную частоту на линии.
#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

Pin pin13_Tone(13, OUTPUT);

static int state = 0;

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();

  // Форма с длительностью: пищит 1.5 c от старта, останавливается сама.
  pin13_Tone.tone(600, Duration::ms(1500));
  ESP_LOGI(TAG_MAIN, "boot beep: tone 600 Hz for 1500 ms");

  // Цикл без duration: старт/стоп вручную — утечек нет. Фаза по 2 c:
  // 440 Hz -> тишина -> 1000 Hz -> тишина -> ...
  onRepeat<Active>(Duration::ms(2000), [](Task &) {
    switch (state) {
      case 0: pin13_Tone.tone(440);  ESP_LOGI(TAG_MAIN, "TONE 440 Hz");  break;
      case 1: pin13_Tone.noTone();   ESP_LOGI(TAG_MAIN, "SILENCE");      break;
      case 2: pin13_Tone.tone(1000); ESP_LOGI(TAG_MAIN, "TONE 1000 Hz"); break;
      case 3: pin13_Tone.noTone();   ESP_LOGI(TAG_MAIN, "SILENCE");      break;
    }
    state = (state + 1) % 4;
  });

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
