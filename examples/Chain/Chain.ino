#include <async/Log.h>
#include <async/Boot.h>
#include <async/Chain.h>

using namespace async;

Boot boot([](Executor * executor) {
  Serial.begin(115200);

  executor->onRepeat(1000, []() {
    info("onRepeat");
  });

  auto ch = chain(0)
    ->delay(Duration(2000))
    ->then([](int val) {
      info("Start value %d", val);
      return val+1;
    })
    ->then([](int val) {
      info("Intermediate value %d", val);
      return val+1;
    })
    ->delay(Duration(2000))
    ->animate(0, 100, Duration(2000), [](int current, int target) {
      info("Animate value %d, value %d", current, target);
      return target + current;
    })
    ->then([](int val) {
      info("Final value %d", val);
      return val;
    });

  executor->add(ch);
});