#include <async/Log.h>
#include <async/Time.h>

// for disable log time uncomment this define
//#define DISABLE_LOG_TIME

using namespace async;

void setup() {
  Serial.begin(115200);
  Time::setSystem(2025, 1, 10, 11, 12, 13, 567);

  trace("trace message %d, %s, %f", millis(), "Hello world!", 0.123);
  debug("debug message %d", millis());
  info("info message %d", millis());
  warn("warn message %d", millis());
  error("error message %d", millis());
}

void loop() {}