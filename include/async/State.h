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
        typedef Function<void(T, T)> OnChangeAllArgsCallback;
        typedef Function<T(T)> GetAndSetAllArgsCallback;

        private:
            volatile T value;
            volatile T prevValue;
            FastList<Task *> tasks;

        public:
            State(T value) : value(value) {}
            Task * onChange(OnChangeAllArgsCallback cbCallback);
            void getAndSet(GetAndSetAllArgsCallback cbCallback);
            T get();
            void set(T value, bool force = false);
    };

    template <typename T>
    inline Task * State<T>::onChange(OnChangeAllArgsCallback cbCallback) {
        auto task =  new Task(Task::DEMAND, [&, cbCallback] () {
            cbCallback(value, prevValue);
        });

        tasks.append(task);
        return task;
    }

    template <typename T>
    inline void State<T>::getAndSet(GetAndSetAllArgsCallback cbCallback) {
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