#pragma once
#include <Arduino.h>
#include <async/Tick.h>
#include <async/Task.h>
#include <async/Executor.h>
#include <async/Function.h>
#include <async/FastList.h>
#include <async/Pin.h>
#include <async/Callbacks.h>
#include <async/Semaphore.h>

//template void attachInterruptArg<void*>(uint8_t&, void (&)(void*), async::Chain<void>*, int&);
//template void attachInterruptArg<int>(uint8_t, void (*)(int), int, int);

namespace async {
    template<typename T = void>
    class Chain;
    
    // Специализация для void
    template<>
    class Chain<void> : public Tick {
        private:
            enum class OpType { DELAY, THEN, SEMAPHORE_WAIT, SEMAPHORE_SKIP, INTERR, LOOP };
            
            struct Operation {
                OpType type;
                VoidCallback callback;
                unsigned long delay;
                unsigned long timeout;
                Semaphore * semaphore;
                Pin * pin;
                Task * task;
            };
        
            FastList<Operation*> operations;
            uint8_t operationCount;
            uint8_t currentOpIndex;
            unsigned long delayStart;
            volatile bool interruptTriggered;
            uint8_t currentInterruptPin;
            bool shouldLoop = false;
        
            void addOperation(Operation * op) {
                operations.append(op);
                operationCount++;
            }
        
            void cleanupInterrupt() {
                if (currentInterruptPin != 255) {
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
            Chain() : operationCount(0), 
                      currentOpIndex(0), delayStart(0), interruptTriggered(false),
                      currentInterruptPin(255) {}
        
            ~Chain() {
                cleanupInterrupt();

                for(int i=0; i < operations.size(); i++) {
                    delete operations[i];
                }
            }
        
            Chain * delay(unsigned long ms) {
                auto op = new Operation();
                op->type = OpType::DELAY;
                op->delay = ms;
                addOperation(op);
                return this;
            }
        
            Chain * then(VoidCallback callback) {
                auto op = new Operation();
                op->type = OpType::THEN;
                op->callback = callback;
                addOperation(op);
                return this;
            }
        
            Chain * semaphoreWaitAcquire(Semaphore* sem) {
                auto op = new Operation();
                op->type = OpType::SEMAPHORE_WAIT;
                op->semaphore = sem;
                addOperation(op);
                return this;
            }

            Chain * semaphoreSkipToStartIfNotAcquired(Semaphore* sem) {
                auto op = new Operation();
                op->type = OpType::SEMAPHORE_SKIP;
                op->semaphore = sem;
                addOperation(op);
                return this;
            }
        
            Chain * interrupt(Pin * pin, int edge, unsigned long timeout = 0xFFFFFFFF) {
                auto op = new Operation();
                op->type = OpType::INTERR;
                op->timeout = timeout;
                op->pin = pin;

                op->task = pin->onInterrupt(edge, [pin, this]() {
                    if(pin->getPin() == this->currentInterruptPin) {
                        this->interruptTriggered = true;
                    }
                });

                addOperation(op);
                return this;
            }
        
            Chain * loop() {
                shouldLoop = true;
                return this;
            }
        
            bool tick() {
                if (currentOpIndex >= operations.size()) {
                    if (shouldLoop) {
                        resetChain();
                        return true;
                    }
                    return false;
                }
                
                Operation * op = operations.get(currentOpIndex); //[currentOpIndex];

                if(op->type == OpType::INTERR) {
                    op->task->tick();
                }
                
                switch (op->type) {
                    case OpType::SEMAPHORE_WAIT:
                        if (!op->semaphore->acquire()) {
                            return true; // Продолжаем ждать
                        }
                        currentOpIndex++;
                        delayStart = millis();
                        return true;

                    case OpType::SEMAPHORE_SKIP:
                        if (!op->semaphore->acquire()) {
                            currentOpIndex = operations.size();
                            return true; // завершаем программу
                        }
                        
                        currentOpIndex++;
                        delayStart = millis();

                        return true;
                        
                    case OpType::DELAY:
                        if (millis() - delayStart < op->delay) {
                            return true;
                        }
                        delayStart = millis();
                        currentOpIndex++;
                        return true;
                        
                    case OpType::THEN:
                        op->callback();
                        currentOpIndex++;
                        return true;
                        
                    case OpType::INTERR:
                        if (currentInterruptPin == 255) {
                            currentInterruptPin = op->pin->getPin();
                            interruptTriggered = false;

                            delayStart = millis();
                        }
                        
                        if (interruptTriggered) {
                            cleanupInterrupt();
                            currentOpIndex++;
                            return true;
                        }
                        
                        if (millis() - delayStart >= op->timeout) {
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
        typedef Function<bool(T)> TypedAgainCallback;

        private:
        enum class OpType { DELAY, THEN, SEMAPHORE_WAIT, SEMAPHORE_SKIP, INTERR, LOOP, CYCLE, AGAIN };
        
        struct Operation {
            OpType type;
            unsigned long delay;
            unsigned long timeout;
            TypedCallback callback;
            TypedAgainCallback againCallback;
            Semaphore * semaphore;
            Pin * pin;
            Task * task;
        };
    
        int operationCount;
        int operationCapacity;
        int currentOpIndex;
        unsigned long delayStart;
        volatile bool interruptTriggered;
        uint8_t currentInterruptPin;
        bool shouldLoop = false;
        bool cycleExitFlag = false;
        T value;
        FastList<Operation *> operations;
    
        void addOperation(Operation * op) {
            operations.append(op);
            operationCount++;
        }
    
        void resetChain() {
            currentOpIndex = 0;
            delayStart = 0;
            interruptTriggered = false;
        }
        
        void cleanupInterrupt() {
            if (currentInterruptPin != 255) {
                currentInterruptPin = 255;
            }
        }
    public:
        Chain(T value) : operationCount(0), operationCapacity(0),
                  currentOpIndex(0), delayStart(0), interruptTriggered(false),
                  currentInterruptPin(255), value(value) {}
    
        ~Chain() {
            cleanupInterrupt();

            for(int i=0; i < operations.size(); i++) {
                delete operations[i];
            }
        }
    
        Chain* delay(unsigned long ms) {
            auto op = new Operation();
            op->type = OpType::DELAY;
            op->delay = ms;
            addOperation(op);
            return this;
        }
    
        Chain* then(TypedCallback callback) {
            auto op = new Operation();
            op->type = OpType::THEN;
            op->callback = callback;
            addOperation(op);
            return this;
        }
    
        Chain* cycle(TypedCallback callback) {
            auto op = new Operation();
            op->type = OpType::CYCLE;
            op->callback = callback;
            addOperation(op);
            return this;
        }
    
        Chain* again(TypedAgainCallback callback) {
            auto op = new Operation();
            op->type = OpType::AGAIN;
            op->againCallback = callback;
            addOperation(op);
            return this;
        }
    
        Chain* semaphoreWaitAcquire(Semaphore* sem) {
            auto op = new Operation();
            op->type = OpType::SEMAPHORE_WAIT;
            op->semaphore = sem;
            addOperation(op);
            return this;
        }

        Chain* semaphoreSkipToStartIfNotAcquired(Semaphore* sem) {
            auto op = new Operation();
            op->type = OpType::SEMAPHORE_SKIP;
            op->semaphore = sem;
            addOperation(op);
            return this;
        }
    
        Chain* interrupt(Pin * pin, int edge, unsigned long timeout = 0xFFFFFFFF) {
            auto op = new Operation();
            op->type = OpType::INTERR;
            op->timeout = timeout;
            op->pin = pin;

            op->task = pin->onInterrupt(edge, [pin, this]() {;
                if(pin->getPin() == this->currentInterruptPin) {
                    this->interruptTriggered = true;
                }
            });
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
    
            Operation * op = operations.get(currentOpIndex); //[currentOpIndex];

            if(op->type == OpType::INTERR) {
                op->task->tick();
            }
            
            switch (op->type) {
                case OpType::SEMAPHORE_WAIT:
                    if (!op->semaphore->acquire()) {
                        return true; // Продолжаем ждать
                    }
                    currentOpIndex++;
                    delayStart = millis();
                    return true;

                case OpType::SEMAPHORE_SKIP:
                    if (!op->semaphore->acquire()) {
                        currentOpIndex = operations.size();
                        return true; // завершаем программу
                    }
                    
                    currentOpIndex++;
                    delayStart = millis();

                    return true;
                    
                case OpType::DELAY:
                    if (millis() - delayStart < op->delay) {
                        return true;
                    }
                    delayStart = millis();
                    currentOpIndex++;
                    return true;
                    
                case OpType::THEN:
                    value = op->callback(value);
                    currentOpIndex++;
                    return true;
                    
                case OpType::AGAIN: {
                    bool result = op->againCallback(value);
                    if(result) {
                        resetChain();
                    }
                    else {
                        currentOpIndex++;
                    }
                    return true;
                }
                    
                case OpType::CYCLE: {
                    T result = op->callback(value);

                    if(result == (T)nullptr) {
                        currentOpIndex++;
                    }
                    else {
                        value = result;
                    }

                    return true;
                }
                    
                case OpType::INTERR:
                    if (currentInterruptPin == 255) {
                        currentInterruptPin = op->pin->getPin();
                        interruptTriggered = false;

                        delayStart = millis();
                    }
                    
                    if (interruptTriggered) {
                        cleanupInterrupt();
                        currentOpIndex++;
                        return true;
                    }
                    
                    if (millis() - delayStart >= op->timeout) {
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