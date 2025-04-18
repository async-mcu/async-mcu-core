#pragma once
#include <async/Executor.h>
#include <async/LinkedList.h>

void setup() __attribute__ ((weak));
void loop() __attribute__ ((weak));

namespace async {
    /**
     * @brief Callback function type for boot initialization
     * @param executor Pointer to the Executor instance
     */
    typedef Function<void(Executor *)> initCallback;

    /**
     * @class Boot
     * @brief Handles system boot initialization with executor setup
     * 
     * This class manages the initialization process for the system,
     * particularly on ESP32 where dual-core support is available.
     * It provides access to the executor and initialization callbacks.
     */
    class Boot {
        private:
            Executor * executor;   ///< Pointer to the executor instance
            initCallback callback; ///< Initialization callback function

        public:
            /**
             * @brief Construct a new Boot instance for core 0
             * @param initCallback The initialization callback function
             */
            Boot(initCallback initCallback);

            /**
             * @brief Initialize the boot process by invoking the callback
             */
            void init() {
                this->callback(executor);
            }

            /**
             * @brief Get the executor instance
             * @return Pointer to the Executor instance
             */
            Executor * getExecutor() {
                return executor;
            }

            /**
             * @brief Get the Boot instance for a specific core
             * @param index Core index (0 or 1)
             * @return Pointer to the Boot instance for the specified core
             */
            static LinkedList<Boot *> & getBoots(int core);

            #ifdef ARDUINO_ARCH_ESP32
            /**
             * @brief Construct a new Boot instance for specific core (ESP32 only)
             * @param core Core number (CORE0 or CORE1)
             * @param cb Initialization callback function
             */
            Boot(byte core, initCallback cb);
            #endif
    };
}