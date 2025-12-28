#include <Arduino.h>
#include <async/Executor.h>

using namespace async;

void setup() {
  Serial.begin(115200); 

  Serial.printf("Before script time %llu ms\n", rts_ms());

  onDelay<Deep>(Duration::ms(2000), CORE0, [](Task *) {
    Serial.printf("onDelay 1 Deep 2000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
    delay(100); // need for printf
  });

  onDelay<Deep>(Duration::ms(2050), CORE1, [](Task *) {
    Serial.printf("onDelay 2 Deep 2000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
    delay(10); // need for printf
  });
  
  onRepeat<Deep>(Duration::ms(2000), CORE1, [](Task *) {
    Serial.printf("onRepeat 2 Deep 2000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
    delay(10); // need for printf
  });

  start();
};

void loop() {

}