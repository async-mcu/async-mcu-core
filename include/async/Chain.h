#pragma once
#include <async/Log.h>
#include <async/Duration.h>
#include <async/Function.h>
#include <async/LinkedList.h>

namespace async {
    template<typename T>
    class Chain : public Tick {
        typedef Function<T(T)> thenCallbackType;
        typedef Function<T(int, T)> animationCallbackType;
    private:
        T value;
        int animateValue;
        enum class ChainState {
            IDLE,
            DELAYING,
            ANIMATING
        };
        
        struct ChainOperation {
            thenCallbackType thenCallback;
            Duration duration;
            int from;
            int to;
            bool isAnimation;
            animationCallbackType animationCallback;

            ChainOperation(thenCallbackType cb, Duration d, T f, T t, bool anim, animationCallbackType animCb)
                : thenCallback(cb), duration(d), from(f), to(t), isAnimation(anim), animationCallback(animCb) {}
        };

        ChainState currentState = ChainState::IDLE;
        unsigned long operationStartTime = 0;
        LinkedList<ChainOperation*> operations;
        int currentOperationIndex = 0;
        bool shouldLoop = false; // Флаг зацикливания
        bool isCancelled = false; // Флаг отмены выполнения

    public:
        Chain(T initialValue) : value(initialValue) {}
        
        ~Chain() {
            // Clean up operations
            for (int i = 0; i < operations.size(); i++) {
                delete operations.get(i);
            }
        }

        Chain* delay(Duration duration) {
            ChainOperation* op = new ChainOperation([](T value) {return value;}, duration, 0, 0, false, [](int value, T value2) {return value2;});
            operations.append(op);
            return this;
        }

        Chain* then(thenCallbackType cb) {
            ChainOperation* op = new ChainOperation(cb, Duration::zero(), 0, 0, false, [](int value, T value2) {return value2;});
            operations.append(op);
            return this;
        }

        Chain* animate(int from, int to, Duration duration, animationCallbackType stepCb) {
            ChainOperation* op = new ChainOperation([](T value) {return value;}, duration, from, to, true, stepCb);
            operations.append(op);
            return this;
        }

        Chain* loop() {
            shouldLoop = true;
            return this;
        }

        bool cancel() override {
            isCancelled = true; // Устанавливаем флаг отмены
            return true;
        }

        bool tick() override {
            // Проверка на отмену выполнения
            if (isCancelled) {
                return false;
            }

            // No more operations
            if (currentOperationIndex >= operations.size()) {
                if (shouldLoop && !isCancelled) {
                    currentOperationIndex = 0;
                    return true;
                }
                
                return false;
            }

            ChainOperation* op = operations.get(currentOperationIndex);

            switch (currentState) {
                case ChainState::IDLE:
                    startOperation(op);
                    break;

                case ChainState::DELAYING:
                    handleDelay(op);
                    break;

                case ChainState::ANIMATING:
                    handleAnimation(op);
                    break;
            }

            return true;
        }

    private:
        void startOperation(ChainOperation* op) {
            operationStartTime = millis();
            
            if (op->isAnimation) {
                currentState = ChainState::ANIMATING;
                animateValue = op->from;
            } else if (op->duration.get() > 0) {
                currentState = ChainState::DELAYING;
            } else {
                executeThenCallback(op);
                moveToNextOperation();
            }
        }

        void handleDelay(ChainOperation* op) {
            if (millis() - operationStartTime >= op->duration.get()) {
                executeThenCallback(op);
                moveToNextOperation();
            }
        }

        void handleAnimation(ChainOperation* op) {
            unsigned long elapsed = millis() - operationStartTime;
            
            if (elapsed >= op->duration.get()) {
                animateValue = op->to;
                this->value = op->animationCallback(animateValue, this->value);
                executeThenCallback(op);
                moveToNextOperation();
            } else {
                float progress = (float)elapsed / op->duration.get();
                animateValue = op->from + (op->to - op->from) * progress;
                this->value = op->animationCallback(animateValue, this->value);
            }
        }

        void executeThenCallback(ChainOperation* op) {
            this->value = op->thenCallback(value);
        }

        void moveToNextOperation() {
            currentOperationIndex++;
            currentState = ChainState::IDLE;
        }
    };

    template<typename T>
    Chain<T>* chain(T value) {
        return new Chain<T>(value);
    }
}