#pragma once

static const char* TAG_EXECUTOR = "EXECUTOR";
static const char* TAG_PIN = "PIN";
static const char* TAG_TASK = "TASK";
static const char* TAG_INTERRUPT = "INTERRUPT";
static const char* TAG_CHAIN = "CHAIN";
static const char* TAG_HTTP_SERVER = "HTTP_SERVER";
static const char* TAG_SETTING = "SETTING";

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define ESP_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#define CONFIG_LOG_DYNAMIC_LEVEL_CONTROL 1
// Library/Arduino15/packages/esp32/tools/esp32-arduino-libs/idf-release_v5.1-632e0c2a/esp32/include/log/include/
#include <esp_log.h>
// Library/Arduino15/packages/esp32/hardware/esp32/3.0.7/cores/esp32/esp32-hal-log.h
#include <esp32-hal-log.h>

// #ifdef CORE_DEBUG_LEVEL
// #undef CORE_DEBUG_LEVEL
// #endif

// #define CORE_DEBUG_LEVEL 3
// #define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

// #undef CONFIG_LOG_MAXIMUM_LEVEL
// #define CONFIG_LOG_MAXIMUM_LEVEL CORE_DEBUG_LEVEL
