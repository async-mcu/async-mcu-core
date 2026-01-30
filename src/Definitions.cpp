#include <async/Definitions.h>

namespace async {
    bool startFlag = false;

    bool isStarted() {
        return startFlag;
    }

    void setStarted(bool started) {
        startFlag = started;
    }
}