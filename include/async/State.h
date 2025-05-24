#pragma once
#include <async/Log.h>
#include <async/Task.h>
#include <async/Function.h>
#include <async/LinkedList.h>
#include <vector>
/**
 * Класс для хранения состояния переменной
 * конструктор содержит тип переменной и её начальное значение (если оно есть)
 * У класса есть методы для получения состояния, обновления состояния и события изменения состояния.
 * Также есть дополнительные методы для 
 */
namespace async { 
    template<typename T>
    class State : public Tick {
        protected:
            T currValue;
            T prevValue;
            Task * task;
            typedef Function<void(T, T)> OnChangeAllArgsCallback;
            typedef Function<T(T)> GetAndSetAllArgsCallback;

        private:
            std::vector<OnChangeAllArgsCallback> callbacks;

        public:
            State(T value) : currValue(value) {
                task = new Task(Task::DEMAND, [&] () {
                    for(int i=0; i < callbacks.size(); i++) {
                        callbacks[i](currValue, prevValue);
                    }
                });
            }
            void onChange(OnChangeAllArgsCallback cbCallback) { 
                callbacks.push_back(cbCallback);
            }

            void getAndSet(GetAndSetAllArgsCallback cbCallback) {
                this->set(cbCallback(this->value));
            }

            virtual T get() {
                return this->currValue;
            }

            virtual void set(T value, bool force = false) {
                if(this->currValue != value || force) {
                    this->prevValue = this->currValue;
                    this->currValue = value;

                    task->demand();
                }
            }

            bool tick() override {
                return task->tick();
            };
    };
}