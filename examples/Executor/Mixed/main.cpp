#include <Arduino.h>
#include <async/Executor.h>

using namespace async;

void setup() {
  Serial.begin(115200); 

  Serial.printf("Before script time %llu ms\n", rts_ms());

  onDelay<Deep>(Duration::ms(2000), CORE0, [](Task *) {
    Serial.printf("onDelay 1 Deep 2000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
    delay(10); // need for printf

    onDelay<Light>(Duration::ms(1300), CORE1, [](Task *) {
      Serial.printf("onDelay 1 Light 1300ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
      delay(10); // need for printf

      // enable wifi
      onDelay<Active>(Duration::ms(500), CORE1, [](Task *) {
        Serial.printf("onDelay 1 Active 500ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
        delay(10); // need for printf
      });
    });
  });

  onRepeat<Deep>(Duration::ms(5000), CORE1, [](Task *) {
    Serial.printf("onDelay 2 Deep 5000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
    delay(10); // need for printf

    onDelay<Light>(Duration::ms(700), CORE1, [](Task *) {
      Serial.printf("onDelay 2 Light 700ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
      delay(10); // need for printf

      // enable wifi
      onDelay<Active>(Duration::ms(300), CORE1, [](Task *) {
        Serial.printf("onDelay 2 Active 300ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
        delay(10); // need for printf
      });
    });
  });

  // onRepeat<Deep>(Duration::ms(13000), CORE1, [](Task *) {
  //   Serial.printf("onDelay 2 Deep 13000ms, time: %llu, core: %d \n", rts_ms(), xPortGetCoreID());
  //   delay(10); // need for printf

  //   onDelay<Active>(Duration::ms(500), CORE1, [](Task *) {
  //     Serial.printf("onDelay 2 Active 500ms, time: %llu, core: %d \n", micros() / 1000ULL, xPortGetCoreID());
  //     delay(10); // need for printf
  //   });
  // });

  start();
};

void loop() {

}