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
        FileStream() : fileSize(0), currentPos(0) {}
    
        bool open(const char* filename, const char* mode = "r") {
            file = SPIFFS.open(filename, mode);
            if (!file) return false;
            
            fileSize = file.size();
            currentPos = 0;
            return true;
        }
    
        void close() {
            file.close();
            fileSize = 0;
            currentPos = 0;
        }
    
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