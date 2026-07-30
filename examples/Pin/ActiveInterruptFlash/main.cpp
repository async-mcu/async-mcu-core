// Stress-тест для issues.md #5: активное прерывание во время flash-операций (NVS).
//
// Суть бага #5: ISR (Pin::ISR → interrupt() → digitalRead()) лежал во flash.
// Когда фронт приходил в окно NVS-записи (spi_flash отключает кэш), ISR читал
// flash-код → Guru Meditation "Cache disabled but cached memory accessed".
//
// Тест автономный (без кнопки): GPIO27 (выход) дёргается по таймеру и через
// ПЕРЕМЫЧКУ 27→14 даёт фронты на pin14 (вход прерывания). Пока летят фронты,
// фоновая задача молотит NVS (окна cache-disable). Часть фронтов гарантированно
// попадает в эти окна.
//   • ДО фикса: краш (Guru Meditation / backtrace) — первый же фронт в окне NVS.
//   • ПОСЛЕ фикса (ISR-цепочка в IRAM, gpio_ll_*): чип не падает, "alive"
//     печатается каждую секунду, FALLING/RISING растут.
//
// ПЕРЕМЫЧКА: проводок GPIO27 → GPIO14.

#include <Async.h>
#include <Preferences.h>
#include "driver/gpio.h"

using namespace async;
static const char* TAG_MAIN = "MAIN";

Pin pin14(14, INPUT_PULLUP);   // вход прерывания; перемычка с GPIO27

volatile int fallCount = 0;
volatile int riseCount = 0;

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();  // выполняет Pin::onInit (setMode/конфиг GPIO) ДО запуска mainLoop

  pin14.addInterrupt<Active>(FALLING, [](Interrupt &) {
    ESP_LOGI(TAG_MAIN, "FALLING #%d", ++fallCount);  // виден сразу — ISR отработал
  });
  pin14.addInterrupt<Active>(RISING,  [](Interrupt &) { riseCount++; });

  // Стимул: GPIO27 (выход) дёргается → перемычкой 27→14 даёт фронты на pin14.
  // ~50 мс на уровень (~20 фронтов/с) — медленно, чтобы не налетать на окно
  // ре-арма прерывания (отдельный баг #3).
  gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
  gpio_set_level(GPIO_NUM_27, 1);
  xTaskCreatePinnedToCore([](void *) {
    int lvl = 1;
    for (;;) {
      lvl ^= 1;
      gpio_set_level(GPIO_NUM_27, lvl);
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }, "stim", 2048, nullptr, 1, nullptr, 1);

  // Фоновая задача: молотит NVS → spi_flash erase/write → отключение кэша.
  xTaskCreatePinnedToCore([](void *) {
    Preferences p;
    p.begin("FLSH", false);
    int v = 0;
    for (;;) {
      p.putInt("k", v++);
      vTaskDelay(1);  // ~1000 записей/с, кормим watchdog
    }
  }, "nvsHammer", 4096, nullptr, 1, nullptr, 1);

  // Если чип жив — эта строка печатается каждую секунду.
  onRepeat<Active>(Duration::ms(1000), [](Task &) {
    ESP_LOGI(TAG_MAIN, "alive: FALLING=%d RISING=%d", fallCount, riseCount);
  });

  ESP_LOGI(TAG_MAIN, "Stress running (jumper GPIO27->GPIO14): autonomous edges + NVS writes");

  startAsync();  // запуск mainLoop — в самом конце (как в setting_simple)
}

void loop() {
  vTaskDelete(NULL);
}
