#include "async/Semaphore.h"

namespace async {

Semaphore::Semaphore(int initialCount, int maximumCount)
    : count(initialCount), maxCount(maximumCount) {
}

bool Semaphore::tryAcquire() {
    int expected = count.load(std::memory_order_relaxed);
    while (expected > 0) {
        if (count.compare_exchange_weak(expected, expected - 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
            return true;
        }
    }
    return false;
}

bool Semaphore::isLock() const {
    return available() == 0;
}

void Semaphore::release() {
    int current = count.load(std::memory_order_relaxed);
    while (current < maxCount) {
        if (count.compare_exchange_weak(current, current + 1,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
            return;
        }
    }
}

int Semaphore::available() const {
    return count.load(std::memory_order_relaxed);
}

} // namespace async
