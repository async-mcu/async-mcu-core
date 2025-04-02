#include "async/Log.h"

void log(int level, const char *format, const char * file, ...) {
    va_list args;
    va_start(args, format);
    //if(level < 2) return;
    //char buf[512];  // Buffer for storing log message
        
    //char new_format[50];
    char buf[512];
    char new_format[256];

    #ifdef ARDUINO_ARCH_ESP32
        sprintf(new_format, "%s [%s\t%d][%s] %s", 
            async::Time::getSystem().toString().c_str(), 
            file, 
            xPortGetCoreID(), 
            level == 0 ? "T" : level == 1 ? "D" : level == 2 ? "I" : level == 3 ? "W" : "E", 
            format);
    #else
        sprintf(new_format, "%s [%s][%s] %s", 
            async::Time::getSystem().toString().c_str(), 
            file, 
            level == 0 ? "T" : level == 1 ? "D" : level == 2 ? "I" : level == 3 ? "W" : "E", 
            format);
    #endif

    vsnprintf(buf, 512, new_format, args);
    Serial.println(buf);

    va_end(args);
}