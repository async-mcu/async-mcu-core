#include "async/Semaphore.h"

namespace async {

Semaphore::Semaphore(int initialCount, int maximumCount)
    : count(initialCount), maxCount(maximumCount) {
    lock = false;
}

bool Semaphore::tryAcquire() {
    if (count > 0 && !lock) {
        --count;
        lock = true;
        return true;
    }
    return false;
}

bool Semaphore::isLock() const {
    return lock;
}

void Semaphore::release() {
    lock = false;
    if (count < maxCount) {
        ++count;
    }
}

int Semaphore::available() const {
    return count;
}

} // namespace async
