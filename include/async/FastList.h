#pragma once

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <async/List.h>
 
namespace async { 
    template <typename T>
    class FastList : public List<T> {
    private:
        volatile T* buffer;
        volatile size_t _size;
        volatile size_t _capacity;
    
        void expand() {
            
            _capacity = _capacity ? _capacity * 2 : 8;
            T* newBuffer = (T*)malloc(_capacity * sizeof(T));
            if (buffer) {
                memcpy(newBuffer, (T*)buffer, _size * sizeof(T));
                free((void*)buffer);
            }
            buffer = (volatile T*)newBuffer;
        }
    
    public:
        FastList(size_t capacity = 0): _size(0), _capacity(capacity), buffer(nullptr) {
            expand();
        }

        ~FastList() {
            if (buffer) free((void*)buffer); 
        }
    
        // Добавление элементов (по значению)
        void append(T item) override {
            if (_size >= _capacity) expand();

            // if(Serial.available()) {
            //     Serial.print ("_capacity ");
            //     Serial.println(_capacity);
            //     Serial.print ("size ");
            //     Serial.println(_size);
            // }
            
            buffer[_size++] = item;
        }
    
        void prepend(T item) override {
            if (_size >= _capacity) expand();
            if (_size > 0) memmove((T*)buffer + 1, (T*)buffer, _size * sizeof(T));
            buffer[0] = item;
            _size++;
        }
    
        // Удаление элементов (по значению)
        bool remove(T item) override {
            for (size_t i = 0; i < _size; ++i) {
                if (buffer[i] == item) {
                    if (i < _size - 1) memmove((T*)buffer + i, (T*)buffer + i + 1, (_size - i - 1) * sizeof(T));
                    _size--;
                    return true;
                }
            }
            return false;
        }
    
        // Поиск (по значению)
        bool contains(T item) override {
            for (size_t i = 0; i < _size; ++i)
                if (buffer[i] == item) return true;
            return false;
        }
    
        // Управление списком
        void clear() override { /* _size = 0; */}
        size_t size() override {
            return _size; 
        }
    
        // Доступ по индексу
        T get(size_t index) override { return buffer[index]; }
    
        // Оператор доступа по индексу
        T operator[](size_t index) override { return buffer[index]; }
    };
}