#include <async/Boot.h>

using namespace async;
Boot* Boot::boots[2] = { NULL, NULL };


#ifdef ARDUINO_ARCH_ESP32
#define millis64() (esp_timer_get_time() / 1000u)
#else
void setup() {
    Boot::getBoot(0)->init();
}
void loop() {
    Boot::getBoot(0)->getExecutor()->tick();
}
#endif


