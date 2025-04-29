#pragma once
#include <Arduino.h>

namespace async {
// Базовый интерфейс потока
class Stream {
    public:
        virtual ~Stream() = default;
        
        // Основные методы Stream
        virtual int available() = 0;
        virtual int read() = 0;
        virtual int peek() = 0;
        virtual size_t read(char* buffer, size_t length) = 0;
        
        // Дополнительные методы для аудиопотоков
        virtual bool seek(size_t pos) = 0;
        virtual size_t position() const = 0;
        virtual size_t size() const = 0;
        
        // Методы записи (оставим пустые реализации)
        virtual size_t write(uint8_t) { return 0; }
        virtual int availableForWrite() { return 0; }
        virtual void flush() {}
    };
}
