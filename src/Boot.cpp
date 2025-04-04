#include <async/Boot.h>

using namespace async;

LinkedList<Boot *> core0; ///< Array of boot instances for each core
LinkedList<Boot *> core1; ///< Array of boot instances for each core

Boot::Boot(initCallback initCallback) {
    core0.append(this);
    this->callback = initCallback;
    this->executor = new Executor();
}

LinkedList<Boot *> & Boot::getBoots(int core) {
    if(core == 0) {
        return core0;
    }
    if(core == 1) {
        return core1;
    }
}


#ifdef ARDUINO_ARCH_ESP32

TaskHandle_t taskLoopCore0;
TaskHandle_t taskLoopCore1;

void setup() {
    xTaskCreatePinnedToCore([] (void * param) 
        { 
            for(int i=0, size = core0.size(); i < size; i++) {
                core0.get(i)->init();
            }

            while (true) {
                for(int i=0, size = core0.size(); i < size; i++) {
                    core0.get(i)->init();
                }
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
            for(int i=0, size = core1.size(); i < size; i++) {
                core1.get(i)->init();
            }

            while (true) {
                for(int i=0, size = core1.size(); i < size; i++) {
                    core1.get(i)->init();
                }
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
    for(int i=0, size = core0.size(); i < size; i++) {
        core0.get(i)->init();
    }
}
void loop() {
    for(int i=0, size = core0.size(); i < size; i++) {
        core0.get(i)->getExecutor()->tick();
    }
}
#endif