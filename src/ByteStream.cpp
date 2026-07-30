#include <string.h>
#include "async/ByteStream.h"

namespace async {

    ByteStream::ByteStream(const unsigned char* buffer, unsigned int size)
        : data(buffer), dataSize(size), currentPos(0) {}

    int ByteStream::available() {
        return dataSize > currentPos ? static_cast<int>(dataSize - currentPos) : 0;
    }

    int ByteStream::read() {
        return available() ? data[currentPos++] : -1;
    }

    int ByteStream::peek() {
        return available() ? data[currentPos] : -1;
    }

    unsigned int ByteStream::read(char* buffer, unsigned int length) {
        unsigned int available = dataSize > currentPos ? dataSize - currentPos : 0;
        unsigned int toRead = available < length ? available : length;
        if (toRead > 0) {
            memcpy(buffer, data + currentPos, toRead);
            currentPos += toRead;
        }
        return toRead;
    }

    bool ByteStream::seek(unsigned int pos) {
        if (pos > dataSize) return false;
        currentPos = pos;
        return true;
    }

    unsigned int ByteStream::position() const {
        return currentPos;
    }

    unsigned int ByteStream::size() const {
        return dataSize;
    }

}
