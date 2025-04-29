#include <async/Stream.h>

namespace async {
// Поток для работы с данными в памяти
class ByteStream : public Stream {
    private:
        const uint8_t* data;
        size_t dataSize;
        size_t currentPos;
    
    public:
        ByteStream(const uint8_t* buffer, size_t size)
            : data(buffer), dataSize(size), currentPos(0) {}
    
        int available() override {
            return dataSize > currentPos ? static_cast<int>(dataSize - currentPos) : 0;
        }
    
        int read() override {
            return available() ? data[currentPos++] : -1;
        }
    
        int peek() override {
            return available() ? data[currentPos] : -1;
        }
    
        size_t read(char* buffer, size_t length) override {
            size_t toRead = min(length, dataSize - currentPos);
            if (toRead > 0) {
                memcpy(buffer, data + currentPos, toRead);
                currentPos += toRead;
            }
            return toRead;
        }
    
        bool seek(size_t pos) override {
            if (pos > dataSize) return false;
            currentPos = pos;
            return true;
        }
    
        size_t position() const override {
            return currentPos;
        }
    
        size_t size() const override {
            return dataSize;
        }
    };
}