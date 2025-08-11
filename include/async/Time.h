#pragma once
#include <async/Tick.h>
#include <async/Duration.h>

/**
 * @brief Writes a 2-digit number to a character buffer
 * @param ptr Pointer to buffer (must have at least 2 bytes available)
 * @param value Number to write (0-99)
 */
inline void writeTwoDigits(char* ptr, int value) {
  ptr[0] = '0' + (value / 10);
  ptr[1] = '0' + (value % 10);
}

/**
 * @brief Writes a 3-digit number to a character buffer
 * @param ptr Pointer to buffer (must have at least 3 bytes available)
 * @param value Number to write (0-999)
 */
inline void writeThreeDigits(char* ptr, int value) {
  ptr[0] = '0' + (value / 100);
  ptr[1] = '0' + ((value / 10) % 10);
  ptr[2] = '0' + (value % 10);
}

/**
 * @brief Writes a 4-digit number to a character buffer
 * @param ptr Pointer to buffer (must have at least 4 bytes available)
 * @param value Number to write (0-9999)
 */
inline void writeFourDigits(char* ptr, int value) {
  ptr[0] = '0' + (value / 1000);
  ptr[1] = '0' + ((value / 100) % 10);
  ptr[2] = '0' + ((value / 10) % 10);
  ptr[3] = '0' + (value % 10);
}

/**
 * @class async::Time
 * @brief Extended time management class with calendar date support
 *
 * @details The Time class provides comprehensive time handling capabilities including:
 * - Unix timestamp management (milliseconds since 1970-01-01)
 * - Calendar date conversions (year/month/day/hour/minute/second)
 * - String formatting for human-readable output
 * - System time management
 * 
 * Inherits from Duration to provide time interval functionality.
 */
namespace async { 
  class Time : public Duration {
    private:
      uint64_t valueMillis; ///< Internal timestamp storage in milliseconds
      static Time SYSTEM_TIME; ///< Static system time reference
      
      ///@name Time Conversion Constants
      ///@{
      static const uint64_t MS_PER_DAY = 86400000ULL; ///< Milliseconds per day
      static const uint64_t MS_PER_HOUR = 3600000ULL; ///< Milliseconds per hour
      static const uint64_t MS_PER_MINUTE = 60000ULL; ///< Milliseconds per minute
      static const uint64_t MS_PER_SECOND = 1000ULL; ///< Milliseconds per second
      ///@}

      /**
       * @brief Check if a year is a leap year
       * @param year Year to check
       * @return true if leap year, false otherwise
       */
      bool isLeapYear(int year) {
        if (year % 4 != 0) return false;
        if (year % 100 != 0) return true;
        return (year % 400 == 0);
      }
      
      /**
       * @brief Get number of days in a month
       * @param month Month (1-12)
       * @param year Year (for leap year calculation)
       * @return Number of days in the month
       */
      int getDaysInMonth(int month, int year) {
          if (month == 2) return isLeapYear(year) ? 29 : 28;
          if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
          return 31;
      }

    public:
      /**
       * @brief Construct a Time object from milliseconds
       * @param ms Milliseconds since Unix epoch (1970-01-01)
       */
      explicit Time(int64_t ms) : Duration(ms) {}
      
      /**
       * @brief Set time using calendar date components
       * @param year Full year (e.g., 2023)
       * @param month Month (1-12)
       * @param day Day of month (1-31)
       * @param hour Hour (0-23)
       * @param minute Minute (0-59)
       * @param second Second (0-59)
       * @param millisecond Millisecond (0-999)
       */
      void setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second, uint16_t millisecond) {
        // Calculate total days since epoch
        uint64_t days = 0;
        
        // Days from full years
        for (uint16_t y = 1970; y < year; ++y) {
            days += isLeapYear(y) ? 366 : 365;
        }
        
        // Days from full months
        for (uint8_t m = 1; m < month; ++m) {
            days += getDaysInMonth(year, m);
        }
        
        // Days from current month
        days += day - 1;

        // Calculate total milliseconds
        uint64_t timestamp = days * MS_PER_DAY;
        timestamp += hour * MS_PER_HOUR;
        timestamp += minute * MS_PER_MINUTE;
        timestamp += second * MS_PER_SECOND;
        timestamp += millisecond;

        this->valueMillis = timestamp;
      }

      /**
       * @brief Convert timestamp to calendar date components
       * @param[out] year Reference to store year
       * @param[out] month Reference to store month (1-12)
       * @param[out] day Reference to store day (1-31)
       * @param[out] hour Reference to store hour (0-23)
       * @param[out] minute Reference to store minute (0-59)
       * @param[out] second Reference to store second (0-59)
       * @param[out] millisecond Reference to store millisecond (0-999)
       */
      void getTime(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second, uint16_t &millisecond) {
        uint64_t timestamp = getTimestamp();
        millisecond = timestamp % 1000;
        uint64_t total_seconds = timestamp / 1000;
        
        second = total_seconds % 60;
        uint64_t total_minutes = total_seconds / 60;
        
        minute = total_minutes % 60;
        uint64_t total_hours = total_minutes / 60;
        
        hour = total_hours % 24;
        uint64_t total_days = total_hours / 24;
    
        // Calculate year
        year = 1970;
        while(true) {
            uint16_t days_in_year = isLeapYear(year) ? 366 : 365;
            if(total_days >= days_in_year) {
                total_days -= days_in_year;
                year++;
            } else {
                break;
            }
        }
    
        // Calculate month and day
        month = 1;
        while(month <= 12) {
            uint8_t dim = getDaysInMonth(year, month);
            if(total_days >= dim) {
                total_days -= dim;
                month++;
            } else {
                break;
            }
        }

        day = total_days + 1;  // Days start at 1
      }

      /**
       * @brief Get current timestamp
       * @return uint64_t Milliseconds since Unix epoch (1970-01-01)
       */
      uint64_t getTimestamp() {
        return millis() + this->valueMillis;
      }

      /**
       * @brief Set timestamp directly
       * @param ms Milliseconds since Unix epoch (1970-01-01)
       */
      void setTimestamp(uint64_t ms) {
        this->valueMillis = ms;
      }

      /**
       * @brief Format date/time into character buffer
       * @param buffer Character buffer (must be at least 24 bytes)
       * @param year Year component
       * @param month Month component
       * @param day Day component
       * @param hour Hour component
       * @param minute Minute component
       * @param second Second component
       * @param millisecond Millisecond component
       * 
       * @note Output format: "YYYY-MM-DD HH:MM:SS.MMM"
       */
      void formatDateTime(char* buffer, uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second, uint16_t &millisecond) {
        writeFourDigits(buffer, year);
        buffer[4] = '-';
        writeTwoDigits(buffer + 5, month);
        buffer[7] = '-';
        writeTwoDigits(buffer + 8, day);
        buffer[10] = ' ';
        writeTwoDigits(buffer + 11, hour);
        buffer[13] = ':';
        writeTwoDigits(buffer + 14, minute);
        buffer[16] = ':';
        writeTwoDigits(buffer + 17, second);
        buffer[19] = '.';
        writeThreeDigits(buffer + 20, millisecond);
        buffer[23] = '\0';
      }

      /**
       * @brief Convert time to formatted string in character buffer
       * @param buffer Character buffer (must be at least 24 bytes)
       */
      void toChar(char * buffer) {
        uint16_t year, millisecond;
        uint8_t month, day, hour, minute, second;
        
        getTime(year, month, day, hour, minute, second, millisecond);
        formatDateTime(buffer, year, month, day, hour, minute, second, millisecond);
      }

      /**
       * @brief Convert time to formatted String object
       * @return String Formatted as "YYYY-MM-DD HH:MM:SS.MMM"
       */
      String toString() {
        uint16_t year, millisecond;
        uint8_t month, day, hour, minute, second;
        char buffer[24];

        getTime(year, month, day, hour, minute, second, millisecond);
        formatDateTime(buffer, year, month, day, hour, minute, second, millisecond);
        
        return String(buffer);
      }

      ///@name Factory Methods
      ///@{

      /**
       * @brief Create Time from calendar components
       * @param year Year (default: 0)
       * @param month Month (1-12, default: 0)
       * @param day Day (1-31, default: 0)
       * @param hour Hour (0-23, default: 0)
       * @param minute Minute (0-59, default: 0)
       * @param second Second (0-59, default: 0)
       * @param millisecond Millisecond (0-999, default: 0)
       * @return Time object
       */
      static Time from(uint16_t year = 0, uint8_t month = 0, uint8_t day = 0, uint8_t hour = 0, uint8_t minute = 0, uint8_t second = 0, uint16_t millisecond = 0) {
        Time time = Time(0);
        time.setTime(year, month, day, hour, minute, second, millisecond);
        return time;
      };

      /**
       * @brief Set system time using calendar components
       * @param year Year (default: 0)
       * @param month Month (1-12, default: 0)
       * @param day Day (1-31, default: 0)
       * @param hour Hour (0-23, default: 0)
       * @param minute Minute (0-59, default: 0)
       * @param second Second (0-59, default: 0)
       * @param millisecond Millisecond (0-999, default: 0)
       */
      static void setSystem(uint16_t year = 0, uint8_t month = 0, uint8_t day = 0, uint8_t hour = 0, uint8_t minute = 0, uint8_t second = 0, uint16_t millisecond = 0) {
        SYSTEM_TIME.setTime(year, month, day, hour, minute, second, millisecond);
      }

      /**
       * @brief Get current system time
       * @return Reference to system Time object
       */
      static Time & getSystem() {
        return SYSTEM_TIME;
      }

      /**
       * @brief Get current time as Time object
       * @return Pointer to new Time object representing current time
       * @note Caller is responsible for memory management
       */
      static Time * now() {
        return new Time(millis());
      };

      /**
       * @brief Get zero time (epoch)
       * @return Pointer to new Time object at zero
       * @note Caller is responsible for memory management
       */
      static Time * zero() {
        return new Time(0);
      };

      /**
       * @brief Create Time from milliseconds
       * @param ms Milliseconds since epoch
       * @return Pointer to new Time object
       * @note Caller is responsible for memory management
       */
      static Time * ms(uint64_t ms) {
        return new Time(ms);
      };
      ///@}
  };
};