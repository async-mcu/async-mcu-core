#pragma once
#include <async/Log.h>
#include <async/Duration.h>
#include <async/Tick.h>
#include <async/Function.h>
#include <async/LinkedList.h>
#include <Arduino.h>

namespace async {


typedef Function<void()> VoidCallback;

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

    
    template<typename T = void>
    class Chain;
    
    // Специализация для void
    template<>
    class Chain<void> : public Tick {
        private:
            enum class OpType { DELAY, THEN, SEMAPHORE, INTERR, LOOP };
            
            struct Operation {
                OpType type;
                unsigned long delay;
                VoidCallback callback;
                Semaphore* semaphore;
                uint8_t pin;
                int mode;
                unsigned long timeout;
            };
        
            Operation* operations;
            int operationCount;
            int operationCapacity;
            int currentOpIndex;
            unsigned long delayStart;
            volatile bool interruptTriggered;
            uint8_t currentInterruptPin;
            bool shouldLoop = false;
        
            static void interruptHandler(void* arg) {
                Chain* self = static_cast<Chain*>(arg);
                if (self) {
                    self->interruptTriggered = true;
                }
            }
        
            void addOperation(const Operation& op) {
                if (operationCount >= operationCapacity) {
                    int newCapacity = operationCapacity ? operationCapacity * 2 : 4;
                    Operation* newOps = new Operation[newCapacity];
                    for (int i = 0; i < operationCount; i++) {
                        newOps[i] = operations[i];
                    }
                    delete[] operations;
                    operations = newOps;
                    operationCapacity = newCapacity;
                }
                operations[operationCount++] = op;
            }
        
            void cleanupInterrupt() {
                if (currentInterruptPin != 255) {
                    detachInterrupt(digitalPinToInterrupt(currentInterruptPin));
                    currentInterruptPin = 255;
                }
            }
        
            void resetChain() {
                currentOpIndex = 0;
                delayStart = 0;
                interruptTriggered = false;
                cleanupInterrupt();
            }
        
        public:
            Chain() : operations(nullptr), operationCount(0), operationCapacity(0),
                      currentOpIndex(0), delayStart(0), interruptTriggered(false),
                      currentInterruptPin(255) {}
        
            ~Chain() {
                cleanupInterrupt();
                delete[] operations;
            }
        
            Chain* delay(unsigned long ms) {
                Operation op;
                op.type = OpType::DELAY;
                op.delay = ms;
                addOperation(op);
                return this;
            }
        
            Chain* then(VoidCallback callback) {
                Operation op;
                op.type = OpType::THEN;
                op.callback = callback;
                addOperation(op);
                return this;
            }
        
            Chain* semaphore(Semaphore* sem) {
                Operation op;
                op.type = OpType::SEMAPHORE;
                op.semaphore = sem;
                addOperation(op);
                return this;
            }
        
            Chain* interrupt(uint8_t pin, int mode, unsigned long timeout = 0xFFFFFFFF) {
                Operation op;
                op.type = OpType::INTERR;
                op.pin = pin;
                op.mode = mode;
                op.timeout = timeout;
                addOperation(op);
                return this;
            }
        
            Chain* loop() {
                shouldLoop = true;
                return this;
            }
        
            bool tick() {
                if (currentOpIndex >= operationCount) {
                    if (shouldLoop) {
                        resetChain();
                        return true;
                    }
                    return false;
                }
        
                Operation& op = operations[currentOpIndex];
                
                switch (op.type) {
                    case OpType::SEMAPHORE:
                        if (!op.semaphore->acquire()) {
                            return true; // Продолжаем ждать
                        }
                        currentOpIndex++;
                        delayStart = millis();
                        return true;
                        
                    case OpType::DELAY:
                        if (millis() - delayStart < op.delay) {
                            return true;
                        }
                        delayStart = millis();
                        currentOpIndex++;
                        return true;
                        
                    case OpType::THEN:
                        op.callback();
                        currentOpIndex++;
                        return true;
                        
                    case OpType::INTERR:
                        if (currentInterruptPin == 255) {
                            currentInterruptPin = op.pin;
                            interruptTriggered = false;
                            attachInterruptArg(
                                digitalPinToInterrupt(op.pin),
                                interruptHandler,
                                this,
                                op.mode
                            );
                            delayStart = millis();
                        }
                        
                        if (interruptTriggered) {
                            cleanupInterrupt();
                            currentOpIndex++;
                            return true;
                        }
                        
                        if (millis() - delayStart >= op.timeout) {
                            cleanupInterrupt();
                            currentOpIndex++;
                            return true;
                        }
                        return true;
                }
        
                return false;
            }
        };

    
    // Шаблонная версия для типизированных цепочек
    template<typename T>
    class Chain : public Tick {
        typedef Function<T(T)> TypedCallback;

        private:
        enum class OpType { DELAY, THEN, SEMAPHORE, INTERR, LOOP };
        
        struct Operation {
            OpType type;
            unsigned long delay;
            TypedCallback callback;
            Semaphore* semaphore;
            uint8_t pin;
            int mode;
            unsigned long timeout;
        };
    
        Operation* operations;
        int operationCount;
        int operationCapacity;
        int currentOpIndex;
        unsigned long delayStart;
        volatile bool interruptTriggered;
        uint8_t currentInterruptPin;
        bool shouldLoop = false;
        T value;
    
        static void interruptHandler(void* arg) {
            Chain* self = static_cast<Chain*>(arg);
            if (self) {
                self->interruptTriggered = true;
            }
        }
    
        void addOperation(const Operation& op) {
            if (operationCount >= operationCapacity) {
                int newCapacity = operationCapacity ? operationCapacity * 2 : 4;
                Operation* newOps = new Operation[newCapacity];
                for (int i = 0; i < operationCount; i++) {
                    newOps[i] = operations[i];
                }
                delete[] operations;
                operations = newOps;
                operationCapacity = newCapacity;
            }
            operations[operationCount++] = op;
        }
    
        void cleanupInterrupt() {
            if (currentInterruptPin != 255) {
                detachInterrupt(digitalPinToInterrupt(currentInterruptPin));
                currentInterruptPin = 255;
            }
        }
    
        void resetChain() {
            currentOpIndex = 0;
            delayStart = 0;
            interruptTriggered = false;
            cleanupInterrupt();
        }
    
    public:
        Chain(T value) : operations(nullptr), operationCount(0), operationCapacity(0),
                  currentOpIndex(0), delayStart(0), interruptTriggered(false),
                  currentInterruptPin(255), value(value) {}
    
        ~Chain() {
            cleanupInterrupt();
            delete[] operations;
        }
    
        Chain* delay(unsigned long ms) {
            Operation op;
            op.type = OpType::DELAY;
            op.delay = ms;
            addOperation(op);
            return this;
        }
    
        Chain* then(TypedCallback callback) {
            Operation op;
            op.type = OpType::THEN;
            op.callback = callback;
            addOperation(op);
            return this;
        }
    
        Chain* semaphore(Semaphore* sem) {
            Operation op;
            op.type = OpType::SEMAPHORE;
            op.semaphore = sem;
            addOperation(op);
            return this;
        }
    
        Chain* interrupt(uint8_t pin, int mode, unsigned long timeout = 0xFFFFFFFF) {
            Operation op;
            op.type = OpType::INTERR;
            op.pin = pin;
            op.mode = mode;
            op.timeout = timeout;
            addOperation(op);
            return this;
        }
    
        Chain* loop() {
            shouldLoop = true;
            return this;
        }
    
        bool tick() {
            if (currentOpIndex >= operationCount) {
                if (shouldLoop) {
                    resetChain();
                    return true;
                }
                return false;
            }
    
            Operation& op = operations[currentOpIndex];
            
            switch (op.type) {
                case OpType::SEMAPHORE:
                    if (!op.semaphore->acquire()) {
                        return true; // Продолжаем ждать
                    }
                    currentOpIndex++;
                    return true;
                    
                case OpType::DELAY:
                    if (millis() - delayStart < op.delay) {
                        return true;
                    }
                    delayStart = millis();
                    currentOpIndex++;
                    return true;
                    
                case OpType::THEN:
                    value = op.callback(value);
                    currentOpIndex++;
                    return true;
                    
                case OpType::INTERR:
                    if (currentInterruptPin == 255) {
                        currentInterruptPin = op.pin;
                        interruptTriggered = false;
                        attachInterruptArg(
                            digitalPinToInterrupt(op.pin),
                            interruptHandler,
                            this,
                            op.mode
                        );
                        delayStart = millis();
                    }
                    
                    if (interruptTriggered) {
                        cleanupInterrupt();
                        currentOpIndex++;
                        return true;
                    }
                    
                    if (millis() - delayStart >= op.timeout) {
                        cleanupInterrupt();
                        currentOpIndex++;
                        return true;
                    }
                    return true;
            }
    
            return false;
        }
    };


// Функции создания
inline Chain<void>* chain() {
    return new Chain<void>();
}

template<typename T>
Chain<T>* chain(T value) {
    return new Chain<T>(value);
}

} // namespace async