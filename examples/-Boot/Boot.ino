#include <async/Log.h>
#include <async/Boot.h>
#include <async/Executor.h>

using namespace async;

Boot boot([](Executor * executor) {
  Serial.begin(115200); 
  info("init boot");

  executor->onRepeat(1000, []() {
    info("Repeat task 1, every 1000 ms");
  });
});