#pragma once

#include <FS.h>
#include <async/Stream.h>

namespace async {
// Поток для работы с файлами
class FileStream : public Stream {
    private:
        File file;
        size_t fileSize;
        size_t currentPos;

    public:
        FileStream(File &file) : file(file), fileSize(file.size()), currentPos(0) {}


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

        unsigned int read(char* buffer, unsigned int length) override {
            size_t bytesRead = file.readBytes(buffer, length);
            currentPos += bytesRead;
            return (unsigned int) bytesRead;
        }

        bool seek(unsigned int pos) override {
            return file.seek(pos);
        }

        unsigned int position() const override {
            return (unsigned int) file.position();
        }

        unsigned int size() const override {
            return (unsigned int) fileSize;
        }
    };
}
