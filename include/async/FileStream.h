#pragma once
#include <async/Stream.h>
#include <SPIFFS.h>

namespace async {
// Поток для работы с файлами
class FileStream : public Stream {
    private:
        File file;
        size_t fileSize;
        size_t currentPos;
    
    public:
        FileStream(File &file) : fileSize(file.size()), currentPos(0), file(file) {}
    
    
        int available() override {
            return file.available();
        }
    
        int read() override {
            int result = file.read();
            if (result != -1) currentPos++;
            return result;
        }
    
        int peek() override {
            return file.peek();
        }
    
        size_t read(char* buffer, size_t length) override {
            size_t bytesRead = file.readBytes(buffer, length);
            currentPos += bytesRead;
            return bytesRead;
        }
    
        bool seek(size_t pos) override {
            if (pos > fileSize) return false;
            bool result = file.seek(pos);
            if (result) currentPos = pos;
            return result;
        }
    
        size_t position() const override {
            return currentPos;
        }
    
        size_t size() const override {
            return fileSize;
        }
    };
}