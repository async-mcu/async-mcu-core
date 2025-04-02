#pragma once
#include <async/Log.h>
#include <async/Task.h>
#include <async/Function.h>
#include <async/LinkedList.h>
/**
 * Класс для хранения состояния переменной
 * конструктор содержит тип переменной и её начальное значение (если оно есть)
 * У класса есть методы для получения состояния, обновления состояния и события изменения состояния.
 * Также есть дополнительные методы для 
 */
namespace async { 
    template<typename T>
    class State {
        typedef Function<void(T, T)> onChangeAllArgsCallback;
        typedef Function<T(T)> getAndSetAllArgsCallback;

        private:
            volatile T value;
            volatile T prevValue;
            LinkedList<Task *> tasks;

        public:
            State(T value) : value(value) {}
            Task * onChange(onChangeAllArgsCallback cbCallback);
            void getAndSet(getAndSetAllArgsCallback cbCallback);
            T get();
            void set(T value, bool force = false);
    };

    template <typename T>
    inline Task * State<T>::onChange(onChangeAllArgsCallback cbCallback) {
        auto task =  new Task(Task::DEMAND, [&, cbCallback] () {
            cbCallback(value, prevValue);
        });

        tasks.append(task);
        return task;
    }

    template <typename T>
    inline void State<T>::getAndSet(getAndSetAllArgsCallback cbCallback) {
        this->set(cbCallback(this->value));
    }

    template <typename T>
    inline T State<T>::get() {
        return this->value;
    }

    template <typename T>
    inline void State<T>::set(T value, bool force) {
        if(this->value != value || force) {
            this->prevValue = this->value;
            this->value = value;

            for(int i=0; i < tasks.size(); i++) {
                tasks.get(i)->demand();
            }
        }
    }
}