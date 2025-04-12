#include <async/Interrupt.h>

using namespace async;


#ifndef ARDUINO_ARCH_ESP32

void ISR(); {
    if (bitRead(EIFR, INTF0) == HIGH) {
        Interrupt::interruptPin = 0;
    }
    if (bitRead(EIFR, INTF1) == HIGH) {
        Interrupt::interruptPin = 1;
    }

    Interrupt::interruptTask->demand();
}

#else

IRAM_ATTR void ISR(void* arg) {
    Task *ptr = (Task*) arg;
    //ets_printf("Button press\n");
	ptr->demand();
}

#endif