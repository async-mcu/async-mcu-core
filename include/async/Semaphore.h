#pragma once

namespace async {
    class Semaphore {
        private:
            int count;
            const int maxCount;
            bool lock = false;
        
        public:
            Semaphore(int initialCount, int maximumCount)
                : count(initialCount), maxCount(maximumCount) {}
        
            bool acquire() {
                if (count > 0 && !lock) {
                    count--;
                    lock = true;
                    return true;
                }
                return false;
            }

            bool locked() {
                return lock;
            }
        
            void release() {
                lock = false;
                if (count < maxCount) {
                    count++;
                }
            }
        
            int available() const { return count; }
        };
}