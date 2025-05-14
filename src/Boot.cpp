#include <async/Boot.h>

using namespace async;

// FastList<InitCallback *> core0boot; ///< Array of boot instances for each core
// FastList<InitCallback *> core1boot; ///< Array of boot instances for each core

// FastList<Executor *> core0executors; ///< Array of boot instances for each core
// FastList<Executor *> core1executors; ///< Array of boot instances for each core

LinkedList<Boot *> core0boot;
LinkedList<Boot *> core1boot;

Executor core0executor;
Executor core1executor;

TaskHandle_t taskLoopCore0;
TaskHandle_t taskLoopCore1;

Boot::Boot(InitCallback initCallback) {
    auto that = this;
    //core0boot.append(that );
}

#ifdef ARDUINO_ARCH_ESP32

Boot::Boot(byte core, InitCallback initCallback) {
    delay(1000);

    if(core == 0) {
        core0boot.append(this);
        //core0boot.push_back(initCallback);
    }
    else {
        core1boot.append(this);
        //core1boot.push_back(initCallback);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("123");

    delay(1000);
    
    Serial.println("init size1");
    Serial.println(core0boot.size());
    Serial.println("init size2");
    Serial.println(core1boot.size());

    xTaskCreatePinnedToCore([] (void * param) { 
        for(int i=0, size = core0boot.size(); i < size; i++) {
            //core0boot.get(i)->init();
        }

        // for (InitCallback initCallback : core0boot) {
        //     initCallback(&core0executor);
        // }

        while (true) {
            core0executor.tick();
            vTaskDelay(1);
        } 
    }, // Task function.
    "",     // name of task. //
    10000,       // Stack size of task //
    NULL,        // parameter of the task //
    1,           // priority of the task //
    &taskLoopCore0,      // Task handle to keep track of created task //
    0);          // pin task to core 0 // 

    xTaskCreatePinnedToCore([] (void * param) {
        for(int i=0, size = core1boot.size(); i < size; i++) {
            //core0boot.get(i)->init();
        }

        while (true) {
            core1executor.tick();
            vTaskDelay(1);
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