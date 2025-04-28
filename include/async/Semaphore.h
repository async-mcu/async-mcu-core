

namespace async {
    class Semaphore {
        private:
            int count;
            const int maxCount;
            bool locked = false;
        
        public:
            Semaphore(int initialCount, int maximumCount)
                : count(initialCount), maxCount(maximumCount) {}
        
            bool acquire() {
                if (count > 0 && !locked) {
                    count--;
                    locked = true;
                    return true;
                }
                return false;
            }
        
            void release() {
                locked = false;
                if (count < maxCount) {
                    count++;
                }
            }
        
            int available() const { return count; }
        };
}