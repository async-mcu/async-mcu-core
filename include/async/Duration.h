#pragma once
#include <Arduino.h>

#ifdef ARDUINO_ARCH_ESP32
/**
 * @brief Get 64-bit millisecond counter (ESP32 specific implementation)
 * @return Current time in milliseconds since boot
 */
#define millis64() (esp_timer_get_time() / 1000u)
#else
/**
 * @brief Get 64-bit millisecond counter (generic implementation)
 * @return Current time in milliseconds since boot
 */
uint64_t millis64();
#endif

/**
 * @class Duration
 * @brief Represents a time duration with conversions between units (microseconds to hours)
 * 
 * @details The Duration class provides a comprehensive way to work with time intervals in various units.
 * It supports creation, storage, manipulation, and comparison of durations with microsecond precision
 * (stored internally as milliseconds). The class is particularly useful for timing operations in
 * asynchronous tasks and event scheduling.
 * 
 * @note All arithmetic operations return new Duration objects rather than modifying existing ones.
 */
namespace async { 
    class Duration {
        protected:
            uint64_t valueMillis; ///< Internal storage in milliseconds (64-bit for extended range)

        public:
            /**
             * @brief Destructor
             */
            ~Duration() {}
            
            /**
             * @brief Construct a new Duration object
             * @param ms Duration value in milliseconds
             */
            Duration(uint64_t ms) {
                valueMillis = ms;
            }

            ///@name Time Unit Constants
            ///@{
            static const byte MICRO   = 0;   ///< Microseconds (μs) unit identifier
            static const byte MILLIS  = 1;   ///< Milliseconds (ms) unit identifier
            static const byte SECONDS = 2;   ///< Seconds (s) unit identifier
            static const byte MINUTES = 3;   ///< Minutes (min) unit identifier
            static const byte HOURS   = 4;   ///< Hours (hr) unit identifier
            ///@}

            /**
             * @brief Set duration value
             * @param ms New duration value in milliseconds
             */
            void set(uint64_t ms) {
                this->valueMillis = ms;
            }

            /**
             * @brief Calculate the absolute difference between two Duration objects
             * @param other The Duration to compare with
             * @return Duration New Duration object representing the absolute difference
             */
            Duration diff(const Duration& other) const {
                return Duration(valueMillis - other.valueMillis);
            }
        
            /**
             * @brief Add another Duration to this one
             * @param other The Duration to add
             * @return Duration New Duration object representing the sum
             */
            Duration add(const Duration& other) const {
                return Duration(valueMillis + other.valueMillis);
            }
        
            /**
             * @brief Subtract another Duration from this one
             * @param other The Duration to subtract
             * @return Duration New Duration object representing the difference
             */
            Duration subtract(const Duration& other) const {
                return Duration(valueMillis - other.valueMillis);
            }

            /**
             * @brief Check if this Duration represents a later time than another
             * @param other The Duration to compare with
             * @return bool True if this Duration is after the other, false otherwise
             */
            bool after(const Duration& other) const {
                return valueMillis > other.valueMillis;
            }

            /**
             * @brief Check if this Duration represents an earlier time than another
             * @param other The Duration to compare with
             * @return bool True if this Duration is before the other, false otherwise
             */
            bool before(const Duration& other) const {
                return valueMillis < other.valueMillis;
            }

            /**
             * @brief Get the duration in the specified time unit
             * @param type Time unit constant (MICRO, MILLIS, SECONDS, etc.)
             * @return uint64_t Duration value converted to the requested unit
             * 
             * @note Conversion details:
             * - MICRO: Returns milliseconds * 1000 (no fractional microseconds)
             * - SECONDS/MINUTES/HOURS: Uses integer division (truncates remainder)
             * - Default unit is milliseconds (MILLIS)
             */
            uint64_t get(int type = MILLIS) {
                switch(type) {
                    case MICRO:   return valueMillis * 1000L;
                    case MILLIS:  return valueMillis;
                    case SECONDS: return valueMillis / 1000L;
                    case MINUTES: return valueMillis / (1000L * 60);
                    case HOURS:   return valueMillis / (1000L * 3600);
                    default:      return valueMillis;
                }
            };

            ///@name Factory Methods
            ///@{

            /**
             * @brief Get current time as a Duration object
             * @return Duration* New Duration object representing current time
             * 
             * @note The caller is responsible for managing the returned pointer
             */
            static Duration now() {
                return Duration(millis64());
            };

            /**
             * @brief Get a zero-length Duration
             * @return Duration* New Duration object representing zero time
             */
            static Duration zero() {
                return Duration(0);
            };

            /**
             * @brief Create a Duration from milliseconds
             * @param ms Time value in milliseconds
             * @return Duration* New Duration object
             */
            static Duration ms(uint64_t ms) {
                return Duration(ms);
            };

            /**
             * @brief Convert the duration to a human-readable string
             * @return String Representation of the duration in milliseconds
             * 
             * @note The string format is a simple decimal number (e.g., "1234")
             * @note Negative values are prefixed with '-' (though unlikely with time durations)
             */
            String toString() {
                uint64_t num = valueMillis;

                static char buf[22];
                char* p = &buf[sizeof(buf)-1];
                *p = '\0';
                do {
                    *--p = '0' + (num%10);
                    num /= 10;
                } while ( num > 0 );
                if( (byte)((num & bit(64))>>63) == 1 ){
                    *--p = '-';
                }
                return p;
            }
    };
}