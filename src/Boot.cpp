#include <async/Boot.h>

using namespace async;
Boot* Boot::boots[2] = { NULL, NULL };


#ifdef ARDUINO_ARCH_ESP32

TaskHandle_t taskLoopCore0;
TaskHandle_t taskLoopCore1;

void setup() {
    xTaskCreatePinnedToCore([] (void * param) 
        { 
            Boot::getBoot(0)->init(); 
            while (true) {
                Boot::getBoot(0)->getExecutor()->tick();
            } 
        }, // Task function.
        "",     // name of task. //
        10000,       // Stack size of task //
        NULL,        // parameter of the task //
        1,           // priority of the task //
        &taskLoopCore0,      // Task handle to keep track of created task //
        0);          // pin task to core 0 // 

    xTaskCreatePinnedToCore([] (void * param) 
        { 
            //Boot::getBoot(1)->init(); 
            while (true) {
                //Boot::getBoot(1)->getExecutor()->tick();
            } 
        }, // Task function.
        "",     // name of task. //
        10000,       // Stack size of task //
        NULL,        // parameter of the task //
        1,           // priority of the task //
        &taskLoopCore1,      // Task handle to keep track of created task //
        1);          // pin task to core 1 //  
}
void loop() {
    vTaskDelete(NULL);
}
#else
void setup() {
    Boot::getBoot(0)->init();
}
void loop() {
    Boot::getBoot(0)->getExecutor()->tick();
}
#endif


