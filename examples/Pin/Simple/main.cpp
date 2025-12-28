#include <Arduino.h>
#include <async/Pin.h>

using namespace async;

//Pin pin13_RTC(13, INPUT_PULLUP);
Pin pin14_RTC(14, INPUT_PULLUP);
//Pin pin33_RTC(33, INPUT_PULLUP);

void setup() {
  Serial.begin(115200);

  Serial.printf("Before script time %llu ms\n", rts_ms());


  // execute function in interrupt
  pin14_RTC.onInterrupt<Deep>(ONLOW, [](Task * task) {
    Serial.printf("pin14_RTC ONLOW, time %llu ms\n", rts_ms());

    onDelay<Light>(Duration::ms(2000), [](Task * task) {
      Serial.printf("2 seconds passed, time %llu ms\n", rts_ms());
    });
  });



  // // execute function in event loop
  // pin14_RTC.onInterrupt(ONLOW, [](Task *) {
  //   Serial.println("pin14_RTC RISING");
  // });

  // // execute function in event loop
  // pin33_RTC.onInterrupt(ONLOW, [](Task *) {
  //   Serial.println("pin33_RTC RISING");
  // });

  start();
};

void loop() {

}