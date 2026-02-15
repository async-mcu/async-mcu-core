#include <async/Boot.h>

namespace async {
    Boot::Boot(std::function<void(void)> callback) {
        onInit(CURRENT_CORE, [callback](Task &) {
            callback();
        });
    }
}

void setup() {
    async::initAsync();
    async::startAsync();
}

void loop() {
    vTaskDelete(NULL);
}