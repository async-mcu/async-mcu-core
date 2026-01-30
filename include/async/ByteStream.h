#pragma once

#include <async/Stream.h>
#include <string.h>

/**
 * @file ByteStream.h
 * @brief Defines the async::ByteStream class for in-memory byte stream operations.
 */

namespace async {

    /**
     * @brief Stream for working with data in memory.
     *
     * Provides methods to read, peek, seek, and get information about a byte buffer.
     */
    class ByteStream : public Stream {
    private:
        const unsigned char* data;    ///< Pointer to the byte buffer.
        unsigned int dataSize;        ///< Size of the buffer.
        unsigned int currentPos;      ///< Current read position.

    public:
        /**
         * @brief Construct a new ByteStream object.
         * @param buffer Pointer to the byte buffer.
         * @param size Size of the buffer.
         */
        ByteStream(const unsigned char* buffer, unsigned int size);

        /**
         * @brief Returns the number of bytes available for reading.
         * @return Number of available bytes.
         */
        int available() override;

        /**
         * @brief Reads the next byte from the stream.
         * @return The byte read, or -1 if none available.
         */
        int read() override;

        /**
         * @brief Peeks at the next byte without removing it from the stream.
         * @return The byte peeked, or -1 if none available.
         */
        int peek() override;

        /**
         * @brief Reads multiple bytes into a buffer.
         * @param buffer Pointer to the buffer.
         * @param length Number of bytes to read.
         * @return Number of bytes actually read.
         */
        unsigned int read(char* buffer, unsigned int length) override;

        /**
         * @brief Seeks to a specific position in the stream.
         * @param pos Position to seek to.
         * @return true if successful, false otherwise.
         */
        bool seek(unsigned int pos) override;

        /**
         * @brief Gets the current position in the stream.
         * @return Current position.
         */
        unsigned int position() const override;

        /**
         * @brief Gets the total size of the stream.
         * @return Size of the stream.
         */
        unsigned int size() const override;
    };
}