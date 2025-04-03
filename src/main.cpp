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
      ->delay(Duration(1000))
      ->then([]() {
        digitalWrite(LED_BUILTIN, LOW);
      })
      ->delay(Duration(1000))
      ->loop();

      auto ch2 = chain<int>(0)
      ->then([](int i) {
        digitalWrite(LED_BUILTIN, HIGH);
        return i;
      })
      ->delay(Duration(1000))
      ->then([](int i) {
        digitalWrite(LED_BUILTIN, LOW);
        return i;
      })
      ->delay(Duration(1000))
      ->loop();

  executor->add(ch);
});