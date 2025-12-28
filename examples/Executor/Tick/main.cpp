#include <Arduino.h>
#include <async/State.h>
#include <async/Executor.h>

using namespace async;


void setup() {
  Serial.begin(115200); 

  Serial.printf("Before script time %llu ms\n", rts_ms());

  
  onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task *) {
    Serial.printf("onDelay 1 Deep 2000ms, time: %llu ms, core: %d \n", rts_ms(), xPortGetCoreID());

    int count = 0;
    uint64_t start_time = micros();
    while(true) {
      count++;

      if(count > 1000) {
        Serial.printf("for count > 1000, lambda time: %llu us, core: %d us\n", micros() - start_time, xPortGetCoreID());
        break;
      }
    }
    
    start_time = micros();

    onTick(CORE0, [count = 0, start_time](Task * that) mutable {
      count++;

      if(count > 1000) {
        Serial.printf("onTick count > 1000, lambda time: %llu us, core: %d us\n", micros() - start_time, xPortGetCoreID());
        that->cancel();
        delay(100);
      }
    });
  });

  start();
};

void loop() {

}