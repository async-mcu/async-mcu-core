#include <Arduino.h>
#include <async/State.h>

using namespace async;

RTC_DATA_ATTR State<int> counter(0);

void setup() {
  Serial.begin(115200); 

  counter.onChange([](int prev, int current) {
    Serial.printf("onChange count of boots: prev %d, current %d \n", prev, current);
  });

  Serial.printf("Count of boots %d \n", counter.set([](int prev) {
    return prev + 1;
  }));

  delay(100);

  esp_sleep_enable_timer_wakeup(1000 * 1000);
  esp_deep_sleep_start();
};

void loop() {

}