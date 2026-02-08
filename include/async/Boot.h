#pragma once

#include <functional>
#include <Async.h>

void setup() __attribute__ ((weak));
void loop() __attribute__ ((weak));

namespace async {

class Boot {
    private:
        //std::function<void(void)> callback;
    public:
        Boot(std::function<void(void)> callback);

};

}