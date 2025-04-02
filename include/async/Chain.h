#pragma once
#include <async/Duration.h>

namespace async { 
    template<typename T>
    class Chain : public Tick {
        private:
            typedef void (*thenCallback)(T current);
            T value;

        public:
            Chain(T value): value(value) {
                //return this;
            }

            Chain& delay(Duration duration) {
                return *this;
            }

            Chain& delay(unsigned long ms) {
                return *this;
            }
            
            Chain& then(thenCallback cb) {
                return *this;
            }
            
            Chain& interrupt(int pin) {
                return *this;
            }

            bool tick() {
                return true;
            }
    };


    template<typename T>
    Chain<T> chain(T value) {
        return Chain<T>(value);
    }
}