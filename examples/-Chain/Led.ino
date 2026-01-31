#include <async/Log.h>
#include <async/Boot.h>
#include <async/Chain.h>

using namespace async;

Boot boot([](Executor * executor) {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  auto ch = chain()
      ->then([]() {
        digitalWrite(LED_BUILTIN, HIGH);
      })
      ->delay(1000)
      ->then([]() {
        digitalWrite(LED_BUILTIN, LOW);
      })
      ->delay(1000)
      ->loop();

  executor->add(ch);
});