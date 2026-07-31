#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include "esp_sleep.h"
#include "esp_task.h"
#include "esp_system.h"
#include "esp_private/panic_internal.h"
#include "esp_heap_trace.h"
#include <async/Duration.h>
#include <async/Task.h>
#include <async/Logging.h>
#include <async/Pin.h>
#include <async/Interrupt.h>

// ext1 wake mode for "wake on LOW". Classic ESP32 only has ALL_LOW (all pins low);
// S2/S3/C3/C6/H2 provide ANY_LOW (any single pin low) and deprecate ALL_LOW. Select by
// target so a pullup-style wake works on a single button where the chip allows it.
#if CONFIG_IDF_TARGET_ESP32
#define ASYNC_EXT1_WAKEUP_LOW ESP_EXT1_WAKEUP_ALL_LOW
#else
#define ASYNC_EXT1_WAKEUP_LOW ESP_EXT1_WAKEUP_ANY_LOW
#endif

namespace async {

    // API планирования задач (onDelay/onRepeat/onOnce/onInit/onTick/onDemand и др.)
    // объявлен в <async/Task.h>. Здесь — только ядро шедулера.
    // mainLoop — внутренняя (static в Executor.cpp).

    void initAsync();
    void startAsync();
}
