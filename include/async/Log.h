#pragma once

#include <Arduino.h>
#include <async/Time.h>

/**
 * @file
 * @brief Logging utilities with different severity levels
 */

#ifndef __FILENAME__
/**
 * @brief Macro to extract just the filename from a full path
 * @param __FILE__ Built-in macro containing current file path
 * @return Pointer to the filename portion of the path
 */
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif

/**
 * @brief Base logging function
 * @param level Log level (0=trace, 1=debug, 2=info, 3=warn, 4=error)
 * @param format Format string (printf-style)
 * @param file Source file name where the log originated
 * @param ... Variable arguments for the format string
 * 
 * @note This is the underlying function used by all logging macros
 * @note Actual implementation should be provided elsewhere
 */
void log(int level, char const * format, char const * file, ...);

///@name Logging Macros
///@{

/**
 * @brief Log a TRACE level message
 * @param format Format string (printf-style)
 * @param ... Variable arguments for the format string
 * 
 * @note Level 0 - Most verbose, for detailed execution tracing
 */
#define trace(format, ...) log(0, format, __FILENAME__, ##__VA_ARGS__)

/**
 * @brief Log a DEBUG level message
 * @param format Format string (printf-style)
 * @param ... Variable arguments for the format string
 * 
 * @note Level 1 - Debug information for development
 */
#define debug(format, ...) log(1, format, __FILENAME__, ##__VA_ARGS__)

/**
 * @brief Log an INFO level message
 * @param format Format string (printf-style)
 * @param ... Variable arguments for the format string
 * 
 * @note Level 2 - General operational information
 */
#define info(format, ...) log(2, format, __FILENAME__, ##__VA_ARGS__)

/**
 * @brief Log a WARNING level message
 * @param format Format string (printf-style)
 * @param ... Variable arguments for the format string
 * 
 * @note Level 3 - Indicates potential issues
 */
#define warn(format, ...) log(3, format, __FILENAME__, ##__VA_ARGS__)

/**
 * @brief Log an ERROR level message
 * @param format Format string (printf-style)
 * @param ... Variable arguments for the format string
 * 
 * @note Level 4 - Serious problems that need attention
 */
#define error(format, ...) log(4, format, __FILENAME__, ##__VA_ARGS__)

///@}