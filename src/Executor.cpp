#include <async/Executor.h>

#ifndef ARDUINO_ARCH_ESP32

static InterruptHandler interruptHandlers[MAX_INTERRUPTS] = { {NULL, NULL} };

void attachInterruptArg(uint8_t interruptNum, void (*userFunc)(void*), void* arg, int mode) {
    if (interruptNum >= MAX_INTERRUPTS) return; // Проверка на валидность
    
    // Сохраняем callback и аргумент
    interruptHandlers[interruptNum].callback = userFunc;
    interruptHandlers[interruptNum].arg = arg;
    
    // Настраиваем прерывание (аналогично стандартному attachInterrupt)
    switch (interruptNum) {
        case 0: // INT0
            EICRA = (EICRA & ~((1 << ISC00) | (1 << ISC01))) | (mode << ISC00);
            EIMSK |= (1 << INT0);
            break;
        case 1: // INT1
            EICRA = (EICRA & ~((1 << ISC10) | (1 << ISC11))) | (mode << ISC10);
            EIMSK |= (1 << INT1);
            break;
        // Можно добавить другие прерывания (PCINT, INT2 и т. д.)
        default:
            break;
    }
}

ISR(INT0_vect) {
    if (interruptHandlers[0].callback != NULL) {
        interruptHandlers[0].callback(interruptHandlers[0].arg);
    }
}

// Обработчик для INT1
ISR(INT1_vect) {
    if (interruptHandlers[1].callback != NULL) {
        interruptHandlers[1].callback(interruptHandlers[1].arg);
    }
}

#endif