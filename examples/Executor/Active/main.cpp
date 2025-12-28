#include <Arduino.h>
#include <async/Executor.h>

using namespace async;

void setup() {
  Serial.begin(115200); 

  Serial.printf("Before script time %llu ms\n", micros() / 1000ULL);

  onDelay<Active>(Duration::ms(2000), CORE0, [](Task *) {
    Serial.printf("onDelay 1 Active 2000ms, time: %llu, core: %d \n", micros() / 1000ULL, xPortGetCoreID());
  });

  onDelay<Active>(Duration::ms(2000), CORE1, [](Task *) {
    Serial.printf("onDelay 2 Active 2000ms, time: %llu, core: %d \n", micros() / 1000ULL, xPortGetCoreID());
  });
  
  onRepeat<Active>(Duration::ms(2000), CORE1, [](Task *) {
    Serial.printf("onRepeat 2 Active 2000ms, time: %llu, core: %d \n", micros() / 1000ULL, xPortGetCoreID());
  });

  start();
};

void loop() {

}