#pragma once
#include <async/Executor.h>
#include <async/FastList.h>
#include <async/Callbacks.h>
#include <list>

#define CORE0 0
#define CORE1 1

void setup() __attribute__ ((weak));
void loop() __attribute__ ((weak));

namespace async {
    class Boot;
    
    /**
     * @brief Callback function type for boot initialization
     * @param executor Pointer to the Executor instance
     */
    typedef std::function<void(Executor *)> InitCallback;
    
    /**
     * @class Boot
     * @brief Handles system boot initialization with executor setup
     * 
     * This class manages the initialization process for the system,
     * particularly on ESP32 where dual-core support is available.
     * It provides access to the executor and initialization callbacks.
     */
    class Boot {
        public:
            /**
             * @brief Construct a new Boot instance for core 0
             * @param initCallback The initialization callback function
             */
            Boot(InitCallback initCallback);

            #ifdef ARDUINO_ARCH_ESP32
            /**
             * @brief Construct a new Boot instance for specific core (ESP32 only)
             * @param core Core number (CORE0 or CORE1)
             * @param cb Initialization callback function
             */
            Boot(byte core, InitCallback cb);
            #endif
    };
}