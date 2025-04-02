#include <async/Log.h>
#include <async/Boot.h>
#include <async/State.h>

using namespace async;

State<int> counter(0);

Boot boot([](Executor * executor) {
  Serial.begin(115200); 
  info("Boot");

  executor->add(counter.onChange([](int current, int prev) {
    info("value changed from %d to %d", prev, current);
  }));

  executor->onDelay(1000, []() {
    counter.getAndSet([](int current) {
      return current * current;
    });
  });

  counter.set(10);
});