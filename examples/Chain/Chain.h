#include <Arduino.h>
#include <async/Chain.h>
#include <async/Scheduler.h>

using namespace async;

Scheduler core1;

void setup() {
  Serial.begin(115200); // Any baud rate should work

  auto ch = chain<int>(0).delay(1000).then([](int value) {
    info("after 1000 ms");
  }).interrupt(10).then([](int value) {
    info("interrupt");
  });

  core1.add(&ch);
}

void loop() {
    core1.tick();
}