#include <async/Log.h>
#include <async/Boot.h>
#include <async/Chain.h>

using namespace async;

Boot boot([](Executor * executor) {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  auto ch = chain(0)
      ->then([](int val) {
        digitalWrite(LED_BUILTIN, HIGH);
        return 0;
      })
      ->delay(Duration(1000))
      ->then([](int val) {
        digitalWrite(LED_BUILTIN, LOW);
        return 0;
      })
      ->delay(Duration(1000))
      ->loop();

  executor->add(ch);
});