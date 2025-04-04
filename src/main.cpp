#include <async/Log.h>
#include <async/Boot.h>
#include <async/Chain.h>

using namespace async;

Boot boot([](Executor * executor) {
  Serial.begin(115200);

});