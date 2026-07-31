#include <async/Globals.h>

namespace async {
    bool startFlag = false;

    bool isStarted() {
        return startFlag;
    }

    void setStarted(bool started) {
        startFlag = started;
    }
}