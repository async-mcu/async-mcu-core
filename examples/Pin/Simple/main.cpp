#include <Arduino.h>
#include <async/Pin.h>

using namespace async;

//Pin pin13_RTC(13, INPUT_PULLUP);
Pin pin14_RTC(14, INPUT_PULLUP);
Pin pin19_RTC(19, INPUT_PULLDOWN);
//Pin pin33_RTC(33, INPUT_PULLUP);

void setup() {
  Serial.begin(115200);

  Serial.printf("Before script time %llu ms\n", rts_ms());

  onRepeat<Light>(Duration::ms(10000), [](Task * task) {
    Serial.printf("10 seconds passed, time %llu ms, mem %d, free stack space0: %d, free stack space1: %d, tasks0 %d, tasks1 %d \n", 
      rts_ms(), 
      ESP.getFreeHeap(), 
      uxTaskGetStackHighWaterMark(taskLoopCore0),
      uxTaskGetStackHighWaterMark(taskLoopCore1),
      tasks[0].size(),
      tasks[1].size());

    for(auto t : tasks[0]) {
      Serial.printf("  task0 mode %d type %d\n", t->getMode(), t->getType());
    } 
    for(auto t : tasks[1]) {
      Serial.printf("  task1 mode %d type %d\n", t->getMode(), t->getType());
    }
  });
  

  pin14_RTC.onInterrupt<Active>(ONLOW, []() {
    Serial.printf("pin14_RTC onInterrupt::ONLOW, time %llu ms\n", rts_ms());
  });

  pin14_RTC.onInterrupt<Active>(RISING, []() {
    Serial.printf("pin14_RTC onInterrupt::RISING, time %llu ms\n", rts_ms());
  });

  pin14_RTC.onInterrupt<Active>(FALLING, []() {
    Serial.printf("pin14_RTC onInterrupt::FALLING, time %llu ms\n", rts_ms());
  });

  pin19_RTC.onInterrupt<Active>(RISING, []() {
    Serial.printf("pin19_RTC onInterrupt::RISING, time %llu ms\n", rts_ms());
  });


  start();
};

void loop() {
  vTaskDelete(NULL);
}