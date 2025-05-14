#pragma once
#include <Arduino.h>
#include <async/Tick.h>
#include <async/Task.h>
#include <async/Executor.h>
#include <async/Function.h>
#include <async/FastList.h>
#include <async/Interrupt.h>
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
                unsigned long delay;
                VoidCallback callback;
                Semaphore* semaphore;
                unsigned long timeout;
                int pin;
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
        
            Chain* delay(unsigned long ms) {
                auto op = new Operation();
                op->type = OpType::DELAY;
                op->delay = ms;
                addOperation(op);
                return this;
            }
        
            Chain* then(VoidCallback callback) {
                auto op = new Operation();
                op->type = OpType::THEN;
                op->callback = callback;
                addOperation(op);
                return this;
            }
        
            Chain* semaphoreWait(Semaphore* sem) {
                auto op = new Operation();
                op->type = OpType::SEMAPHORE_WAIT;
                op->semaphore = sem;
                addOperation(op);
                return this;
            }

            Chain* semaphoreSkip(Semaphore* sem) {
                auto op = new Operation();
                op->type = OpType::SEMAPHORE_SKIP;
                op->semaphore = sem;
                addOperation(op);
                return this;
            }
        
            Chain* interrupt(Interrupt * interrupt, unsigned long timeout = 0xFFFFFFFF) {
                auto op = new Operation();
                op->type = OpType::INTERR;
                op->timeout = timeout;
                op->pin = interrupt->getPin();

                op->task = interrupt->onInterrupt([interrupt, this]() {
                    if(interrupt->getPin() == this->currentInterruptPin) {
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
                            currentInterruptPin = op->pin;
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

        private:
        enum class OpType { DELAY, THEN, SEMAPHORE_WAIT, SEMAPHORE_SKIP, INTERR, LOOP };
        
        struct Operation {
            OpType type;
            unsigned long delay;
            TypedCallback callback;
            Semaphore* semaphore;
            uint8_t pin;
            unsigned long timeout;
            Task * task;
        };
    
        int operationCount;
        int operationCapacity;
        int currentOpIndex;
        unsigned long delayStart;
        volatile bool interruptTriggered;
        uint8_t currentInterruptPin;
        bool shouldLoop = false;
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
    
        Chain* semaphoreWait(Semaphore* sem) {
            auto op = new Operation();
            op->type = OpType::SEMAPHORE_WAIT;
            op->semaphore = sem;
            addOperation(op);
            return this;
        }

        Chain* semaphoreSkip(Semaphore* sem) {
            auto op = new Operation();
            op->type = OpType::SEMAPHORE_SKIP;
            op->semaphore = sem;
            addOperation(op);
            return this;
        }
    
        Chain* interrupt(Interrupt * interrupt, unsigned long timeout = 0xFFFFFFFF) {
            auto op = new Operation();
            op->type = OpType::INTERR;
            op->timeout = timeout;
            op->pin = interrupt->getPin();
            op->task = interrupt->onInterrupt([interrupt, this]() {;
                if(interrupt->getPin() == this->currentInterruptPin) {
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
                    
                case OpType::INTERR:
                    if (currentInterruptPin == 255) {
                        currentInterruptPin = op->pin;
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