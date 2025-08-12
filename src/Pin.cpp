#include <async/Pin.h>

using namespace async;

IRAM_ATTR void ISR(void* arg) {
    DemandTask<bool> * ptr = (DemandTask<bool>*) arg;
    //ets_printf("Button press\n");
	ptr->demand(gpio_get_level((gpio_num_t) ptr->getParam()));
}