#pragma once

// to del -> use vector
namespace async { 
    template <typename T>
    class List {
        public:
            virtual ~List() = default;
            
            // Добавление элементов
            virtual void append(T item) = 0;
            virtual void prepend(T item) = 0;
            
            // Удаление элементов
            virtual bool remove(T item) = 0;
            virtual void clear() = 0;
            
            // Поиск
            virtual bool contains(T item) = 0;
            virtual T get(size_t index) = 0;
            
            // Размер
            virtual size_t size() = 0;
            
            // Доступ по индексу (не для всех реализаций)
            virtual T operator[](size_t index) = 0;
    };
}
