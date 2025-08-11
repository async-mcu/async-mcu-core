#pragma once
#include <Arduino.h>

/**
 * @file Stream.h
 * @brief Defines the async::Stream interface for asynchronous data streams.
 */

namespace async {

    /**
     * @brief Base interface for asynchronous data streams.
     *
     * Provides methods for reading, peeking, seeking, and writing data.
     * Designed to support both generic and audio streams.
     */
    class Stream {
    public:
        /**
         * @brief Virtual destructor.
         */
        virtual ~Stream() = default;
        
        /**
         * @brief Returns the number of bytes available for reading.
         * @return Number of available bytes.
         */
        virtual int available() = 0;

        /**
         * @brief Reads the next byte from the stream.
         * @return The byte read, or -1 if none available.
         */
        virtual int read() = 0;

        /**
         * @brief Peeks at the next byte without removing it from the stream.
         * @return The byte peeked, or -1 if none available.
         */
        virtual int peek() = 0;

        /**
         * @brief Reads multiple bytes into a buffer.
         * @param buffer Pointer to the buffer.
         * @param length Number of bytes to read.
         * @return Number of bytes actually read.
         */
        virtual size_t read(char* buffer, size_t length) = 0;
        
        /**
         * @brief Seeks to a specific position in the stream.
         * @param pos Position to seek to.
         * @return true if successful, false otherwise.
         */
        virtual bool seek(size_t pos) = 0;

        /**
         * @brief Gets the current position in the stream.
         * @return Current position.
         */
        virtual size_t position() const = 0;

        /**
         * @brief Gets the total size of the stream.
         * @return Size of the stream.
         */
        virtual size_t size() const = 0;
        
        /**
         * @brief Writes a byte to the stream.
         * @param data Byte to write.
         * @return Number of bytes written (default 0).
         */
        virtual size_t write(uint8_t data) { return 0; }

        /**
         * @brief Returns the number of bytes available for writing.
         * @return Number of bytes available (default 0).
         */
        virtual int availableForWrite() { return 0; }

        /**
         * @brief Flushes the stream (default does nothing).
         */
        virtual void flush() {}
    };
}