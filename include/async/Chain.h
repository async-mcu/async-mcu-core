#pragma once
#include <async/Log.h>
#include <async/Duration.h>
#include <async/Function.h>
#include <async/LinkedList.h>

namespace async {

// Базовый шаблон для не-void типов
template<typename T>
class Chain : public Tick {
    typedef Function<T(T)> thenCallbackType;
    typedef Function<T(int, T)> animationCallbackType;
    
private:
    T value;
    int animateValue;
    enum class ChainState { IDLE, DELAYING, ANIMATING };
    
    struct ChainOperation {
        thenCallbackType thenCallback;
        Duration duration;
        int from;
        int to;
        bool isAnimation;
        animationCallbackType animationCallback;

        ChainOperation(thenCallbackType cb, Duration d, int f, int t, bool anim, animationCallbackType animCb)
            : thenCallback(cb), duration(d), from(f), to(t), isAnimation(anim), animationCallback(animCb) {}
    };

    ChainState currentState = ChainState::IDLE;
    unsigned long operationStartTime = 0;
    LinkedList<ChainOperation*> operations;
    int currentOperationIndex = 0;
    bool shouldLoop = false;
    bool isCancelled = false;

public:
    Chain(T initialValue) : value(initialValue) {}
    
    ~Chain() {
        for (int i = 0; i < operations.size(); i++) {
            delete operations.get(i);
        }
    }

    Chain* delay(Duration duration) {
        auto op = new ChainOperation(
            [](T v) { return v; }, 
            duration, 
            0, 0, false, 
            [](int, T v) { return v; }
        );
        operations.append(op);
        return this;
    }

    Chain* then(thenCallbackType cb) {
        auto op = new ChainOperation(
            cb, 
            Duration::zero(), 
            0, 0, false, 
            [](int, T v) { return v; }
        );
        operations.append(op);
        return this;
    }

    Chain* animate(int from, int to, Duration duration, animationCallbackType stepCb) {
        auto op = new ChainOperation(
            [](T v) { return v; }, 
            duration, 
            from, to, true, 
            stepCb
        );
        operations.append(op);
        return this;
    }

    Chain* loop() {
        shouldLoop = true;
        return this;
    }

    bool cancel() override {
        isCancelled = true;
        return true;
    }

    bool tick() override {
        if (isCancelled) return false;
        if (currentOperationIndex >= operations.size()) {
            if (shouldLoop) {
                currentOperationIndex = 0;
                return true;
            }
            return false;
        }

        auto op = operations.get(currentOperationIndex);
        switch (currentState) {
            case ChainState::IDLE: startOperation(op); break;
            case ChainState::DELAYING: handleDelay(op); break;
            case ChainState::ANIMATING: handleAnimation(op); break;
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
            value = op->thenCallback(value);
            moveToNextOperation();
        }
    }

    void handleDelay(ChainOperation* op) {
        if (millis() - operationStartTime >= op->duration.get()) {
            value = op->thenCallback(value);
            moveToNextOperation();
        }
    }

    void handleAnimation(ChainOperation* op) {
        unsigned long elapsed = millis() - operationStartTime;
        if (elapsed >= op->duration.get()) {
            animateValue = op->to;
            value = op->animationCallback(animateValue, value);
            moveToNextOperation();
        } else {
            float progress = (float)elapsed / op->duration.get();
            animateValue = op->from + (op->to - op->from) * progress;
            value = op->animationCallback(animateValue, value);
        }
    }

    void moveToNextOperation() {
        currentOperationIndex++;
        currentState = ChainState::IDLE;
    }
};

// Специализация для void
template<>
class Chain<void> : public Tick {
    typedef Function<void()> thenCallbackType;
    typedef Function<void(int)> animationCallbackType;
    
private:
    int animateValue;
    enum class ChainState { IDLE, DELAYING, ANIMATING };
    
    struct ChainOperation {
        thenCallbackType thenCallback;
        Duration duration;
        int from;
        int to;
        bool isAnimation;
        animationCallbackType animationCallback;

        ChainOperation(thenCallbackType cb, Duration d, int f, int t, bool anim, animationCallbackType animCb)
            : thenCallback(cb), duration(d), from(f), to(t), isAnimation(anim), animationCallback(animCb) {}
    };

    ChainState currentState = ChainState::IDLE;
    unsigned long operationStartTime = 0;
    LinkedList<ChainOperation*> operations;
    int currentOperationIndex = 0;
    bool shouldLoop = false;
    bool isCancelled = false;

public:
    Chain() = default;
    
    ~Chain() {
        for (int i = 0; i < operations.size(); i++) {
            delete operations.get(i);
        }
    }

    Chain* delay(Duration duration) {
        auto op = new ChainOperation(
            [](){}, 
            duration, 
            0, 0, false, 
            [](int){}
        );
        operations.append(op);
        return this;
    }

    Chain* then(thenCallbackType cb) {
        auto op = new ChainOperation(
            cb, 
            Duration::zero(), 
            0, 0, false, 
            [](int){}
        );
        operations.append(op);
        return this;
    }

    Chain* animate(int from, int to, Duration duration, animationCallbackType stepCb) {
        auto op = new ChainOperation(
            [](){}, 
            duration, 
            from, to, true, 
            stepCb
        );
        operations.append(op);
        return this;
    }

    Chain* loop() {
        shouldLoop = true;
        return this;
    }

    bool cancel() override {
        isCancelled = true;
        return true;
    }

    bool tick() override {
        if (isCancelled) return false;
        if (currentOperationIndex >= operations.size()) {
            if (shouldLoop) {
                currentOperationIndex = 0;
                return true;
            }
            return false;
        }

        auto op = operations.get(currentOperationIndex);
        switch (currentState) {
            case ChainState::IDLE: startOperation(op); break;
            case ChainState::DELAYING: handleDelay(op); break;
            case ChainState::ANIMATING: handleAnimation(op); break;
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
            op->thenCallback();
            moveToNextOperation();
        }
    }

    void handleDelay(ChainOperation* op) {
        if (millis() - operationStartTime >= op->duration.get()) {
            op->thenCallback();
            moveToNextOperation();
        }
    }

    void handleAnimation(ChainOperation* op) {
        unsigned long elapsed = millis() - operationStartTime;
        if (elapsed >= op->duration.get()) {
            animateValue = op->to;
            op->animationCallback(animateValue);
            moveToNextOperation();
        } else {
            float progress = (float)elapsed / op->duration.get();
            animateValue = op->from + (op->to - op->from) * progress;
            op->animationCallback(animateValue);
        }
    }

    void moveToNextOperation() {
        currentOperationIndex++;
        currentState = ChainState::IDLE;
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