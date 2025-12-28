#include <Arduino.h>
#include <async/Executor.h>

using namespace async;

void setup() {
  Serial.begin(115200); 

  Serial.printf("Before script time %llu ms\n", rts_ms());

  auto demand = onDemand(CORE1, [](Task *) {
    Serial.printf("onDemand, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
  });

  onDelay<Deep>(Duration::ms(2000), CORE0, [&demand](Task *) {
    Serial.printf("onDelay 1 Deep 2000ms, time: %llu, core: %d \n",  rts_ms(), xPortGetCoreID());
    demand->execute();
    delay(10); // need for printf
  });


  start();
};

void loop() {

}