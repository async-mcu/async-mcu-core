#include <Arduino.h>
#include <async/Pin.h>

using namespace async;

Pin pin12(12, OUTPUT, HIGH);
Pin pin14(14, INPUT);
Pin pin15(15, OUTPUT, LOW);

// simple buttons
Pin inputPullup(23, INPUT_PULLUP);
Pin inputPullDown(22, INPUT_PULLDOWN);

// buzzer
Pin buzzer(26, OUTPUT, LOW);

void setup() {
  Serial.begin(115200);
  
  // execute function in interrupt
  inputPullDown.onInterrupt<Deep>(RISING, [](Task * task) {
    Serial.println("inputPullDown");
  });

  // execute function in event loop
  inputPullDown.onInterrupt<Deep>(RISING, [](Task *) {
    Serial.println("inputPullDown");
  });
};

void loop() {

}