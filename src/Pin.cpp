#include <async/Pin.h>

using namespace async;

IRAM_ATTR void ISR(void* arg) {
    Task *ptr = (Task*) arg;
    //ets_printf("Button press\n");
	ptr->demand();
}
