#pragma once

#include <stdint.h>
#include "esp_timer.h"

/**
 * @class Duration
 * @brief Represents a time duration with conversions between units (microseconds to hours)
 * 
 * @details The Duration class provides a comprehensive way to work with time intervals in various units.
 * It supports creation, storage, manipulation, and comparison of durations with microsecond precision
 * (stored internally as microseconds). The class is particularly useful for timing operations in
 * asynchronous tasks and event scheduling.
 * 
 * @note All arithmetic operations return new Duration objects rather than modifying existing ones.
 */
namespace async {

class Duration {
    protected:
        uint64_t valueMicros; ///< Internal storage in microseconds (64-bit for extended range)

        /**
         * @brief Construct a new Duration object
         * @param us Duration value in microseconds
         *
         * ### Example
         * ```cpp
         * Duration d1(1000); // 1000 microseconds
         * Duration d2 = Duration::ms(1); // 1 millisecond = 1000 microseconds
         * ```
         */
        Duration(uint64_t us);

    public:
        /**
         * @brief Destructor
         *
         * ### Example
         * ```cpp
         * Duration* d = new Duration(1000);
         * delete d;
         * ```
         */
        ~Duration();



        /**
         * @brief Set duration value
         * @param us New duration value in microseconds
         *
         * ### Example
         * ```cpp
         * Duration d(0);
         * d.set(5000); // Set to 5000 microseconds
         * ```
         */
        void set(uint64_t us);

        /**
         * @brief Calculate the absolute difference between two Duration objects
         * @param other The Duration to compare with
         * @return Duration New Duration object representing the absolute difference
         *
         * ### Example
         * ```cpp
         * Duration d1(10000);
         * Duration d2(7000);
         * Duration diff = d1.diff(d2); // diff = 3000 microseconds
         * ```
         */
        Duration diff(const Duration& other) const;

        /**
         * @brief Add another Duration to this one
         * @param other The Duration to add
         * @return Duration New Duration object representing the sum
         *
         * ### Example
         * ```cpp
         * Duration d1(1000);
         * Duration d2(2000);
         * Duration sum = d1.add(d2); // sum = 3000 microseconds
         * ```
         */
        Duration add(const Duration& other) const;

        /**
         * @brief Subtract another Duration from this one
         * @param other The Duration to subtract
         * @return Duration New Duration object representing the difference
         *
         * ### Example
         * ```cpp
         * Duration d1(5000);
         * Duration d2(2000);
         * Duration diff = d1.subtract(d2); // diff = 3000 microseconds
         * ```
         */
        Duration subtract(const Duration& other) const;

        /**
         * @brief Check if this Duration represents a later time than another
         * @param other The Duration to compare with
         * @return bool True if this Duration is after the other, false otherwise
         *
         * ### Example
         * ```cpp
         * Duration d1(10000);
         * Duration d2(5000);
         * bool isAfter = d1.after(d2); // true
         * ```
         */
        bool after(const Duration& other) const;

        /**
         * @brief Check if this Duration represents an earlier time than another
         * @param other The Duration to compare with
         * @return bool True if this Duration is before the other, false otherwise
         *
         * ### Example
         * ```cpp
         * Duration d1(1000);
         * Duration d2(5000);
         * bool isBefore = d1.before(d2); // true
         * ```
         */
        bool before(const Duration& other) const;

        uint64_t us();

        uint64_t ms();

        uint64_t sec();

        /**


        ///@name Factory Methods
        ///@{

        /**
         * @brief Get current time as a Duration object (microseconds)
         * @return Duration New Duration object representing current time
         *
         * ### Example
         * ```cpp
         * Duration now = Duration::now();
         * Serial.println(now.get());
         * ```
         */
        static Duration now();

        /**
         * @brief Get maximum possible Duration
         * @return Duration New Duration object representing maximum value
         *
         * ### Example
         * ```cpp
         * Duration maxDur = Duration::maximum();
         * ```
         */
        static Duration maximum();

        /**
         * @brief Get a zero-length Duration
         * @return Duration New Duration object representing zero time
         *
         * ### Example
         * ```cpp
         * Duration zeroDur = Duration::zero();
         * ```
         */
        static Duration zero();

        static Duration * us(uint32_t us);

        /**
         * @brief Create a Duration from milliseconds
         * @param ms Time value in milliseconds
         * @return Duration New Duration object
         *
         * ### Example
         * ```cpp
         * Duration d = Duration::ms(2); // 2000 microseconds
         * ```
         */
        static Duration * ms(uint32_t ms);

        /**
         * @brief Convert the duration to a human-readable string
         * @return String Representation of the duration in microseconds
         * 
         * @note The string format is a simple decimal number (e.g., "1234567")
         * @note Negative values are prefixed with '-' (though unlikely with time durations)
         *
         * ### Example
         * ```cpp
         * Duration d(1234567);
         * String s = d.toString(); // "1234567"
         * Serial.println(s);
         * ```
         */
        // String toString() {
        //     uint64_t num = valueMicros;

        //     static char buf[22];
        //     char* p = &buf[sizeof(buf)-1];
        //     *p = '\0';
        //     do {
        //         *--p = '0' + (num%10);
        //         num /= 10;
        //     } while ( num > 0 );
        //     if( (byte)((num & bit(64))>>63) == 1 ){
        //         *--p = '-';
        //     }
        //     return p;
        // }

        /**
         * @brief Assignment operator
         *
         * ### Example
         * ```cpp
         * Duration d1(1000);
         * Duration d2 = d1;
         * ```
         */
        Duration& operator=(const Duration& other);

        /**
         * @brief Comparison operators with Duration
         *
         * ### Example
         * ```cpp
         * Duration d1(1000);
         * Duration d2(2000);
         * bool eq = d1 == d2; // false
         * bool neq = d1 != d2; // true
         * bool less = d1 < d2; // true
         * bool greater = d2 > d1; // true
         * bool leq = d1 <= d2; // true
         * bool geq = d2 >= d1; // true
         * ```
         */
        bool operator==(const Duration& other) const;
        bool operator!=(const Duration& other) const;
        bool operator<(const Duration& other) const;
        bool operator>(const Duration& other) const;
        bool operator<=(const Duration& other) const;
        bool operator>=(const Duration& other) const;
        operator uint64_t() const { return valueMicros; }
        
        /**
         * @brief Arithmetic operators with Duration and uint64_t
         *
         * ### Example
         * ```cpp
         * Duration d1(1000);
         * Duration d2(2000);
         * Duration sum = d1 + d2; // 3000
         * Duration diff = d2 - d1; // 1000
         * Duration prod = d1 * d2; // 2,000,000
         * Duration quot = d2 / d1; // 2
         * Duration prod2 = d1 * 3; // 3000
         * Duration quot2 = d2 / 2; // 1000
         * Duration sum2 = d1 + 500; // 1500
         * Duration diff2 = d2 - 500; // 1500
         * ```
         */
        Duration operator+(const Duration& other) const;
        Duration operator-(const Duration& other) const;
        Duration operator*(const Duration& other) const;
        Duration operator/(const Duration& other) const;

        Duration operator*(uint64_t factor) const;
        Duration operator/(uint64_t divisor) const;
        Duration operator+(uint64_t other) const;
        Duration operator-(uint64_t other) const;
};
}