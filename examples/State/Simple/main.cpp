#include <Arduino.h>
#include <async/State.h>

using namespace async;

State<int> value(0);

void setup() {
  Serial.begin(115200); 

  value.onChange([](int prev, int current) {
    Serial.printf("onChange 1 value: prev %d, current %d \n", prev, current);
  });

  value.onChange([](int prev, int current) {
    Serial.printf("onChange 2 value: prev %d, current %d \n", prev, current);
  });

  Serial.printf("value %d \n", value.set([](int prev) {
    return prev + 1;
  }));
};

void loop() {

}