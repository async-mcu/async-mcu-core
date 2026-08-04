#pragma once

// Globals.h — общие определения (макросы/enum, бывш. Definitions.h) И extern-объявления
// globals планировщика (бывш. ExecutorInternal.h). Определения globals — в Executor.cpp;
// используются также в Task.cpp.

#include "esp_cpu.h"
#include <cstddef>
#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys/time.h"

// === Макросы (бывш. Definitions.h) ===

// Генерируем выражение с || для всех элементов
#define BOOL_OR_1(a) (a[0])
#define BOOL_OR_2(a) (a[0] || a[1])
#define BOOL_OR_3(a) (a[0] || a[1] || a[2])
#define BOOL_OR_4(a) (a[0] || a[1] || a[2] || a[3])
#define BOOL_OR_N(n, a) BOOL_OR_##n(a)
#define BOOL_OR(a, n) BOOL_OR_N(n, a)

// Генерируем выражение с && для всех элементов
#define BOOL_AND_1(a) (a[0])
#define BOOL_AND_2(a) (a[0] && a[1])
#define BOOL_AND_3(a) (a[0] && a[1] && a[2])
#define BOOL_AND_4(a) (a[0] && a[1] && a[2] && a[3])
#define BOOL_AND_N(n, a) BOOL_AND_##n(a)
#define BOOL_AND(a, n) BOOL_AND_N(n, a)

// Минимум в массиве (inline-шаблон: без повторного вычисления аргументов)
template <typename T>
inline T minInArray(const T * arr, size_t n) {
    T best = arr[0];
    for (size_t i = 1; i < n; i++) {
        if (arr[i] < best) best = arr[i];
    }
    return best;
}

// Инициализация массива одинаковыми значениями
#define INIT_ARRAY_1(val) {val}
#define INIT_ARRAY_2(val) {val, val}
#define INIT_ARRAY_3(val) {val, val, val}
#define INIT_ARRAY_4(val) {val, val, val, val}
#define INIT_ARRAY_5(val) {val, val, val, val, val}
#define INIT_ARRAY_6(val) {val, val, val, val, val, val}
#define INIT_ARRAY_N(n, val) INIT_ARRAY_##n(val)
#define INIT_ARRAY(val, n) INIT_ARRAY_N(n, val)

#define CURRENT_CORE (esp_cpu_get_core_id() == 0 ? CORE0 : CORE1)
#define DEEP_TASKS_STACK 20

// Arduino-style константы
#define LOW               0x0
#define HIGH              0x1
#define INPUT             0x01
#define OUTPUT            0x03
#define PULLUP            0x04
#define INPUT_PULLUP      0x05
#define PULLDOWN          0x08
#define INPUT_PULLDOWN    0x09
#define OPEN_DRAIN        0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define ANALOG            0xC0
#define OUTPUT_ANALOG     0xC3  // OUTPUT (0x03) | ANALOG (0xC0)

#define RISING    GPIO_INTR_POSEDGE
#define FALLING   GPIO_INTR_NEGEDGE
#define CHANGE    GPIO_INTR_ANYEDGE
#define ONLOW     GPIO_INTR_LOW_LEVEL
#define ONHIGH    GPIO_INTR_HIGH_LEVEL

namespace async {
    class Task;   // forward — для extern coreTasks[] и др.; определение в Task.h
    class Interrupt;   // forward — для globalInterruptParams; определение в Interrupt.h

    // Определения для асинхронного выполнения задач
    enum SleepMode {
        None,
        Deep,
        Light,
        Active
    };

    enum Type {
        REPEAT = 0,
        DELAY = 10,
        DEMAND = 20,
        TICK = 30,
        ONCE = 40,
        INTERR = 50,
        INIT = 60
    };

    enum Core {
        CORE0 = 0,
        CORE1 = 1
    };

    inline char const* modeToStr(SleepMode mode) {
        switch(mode) {
          case None: return "None";
          case Deep: return "Deep";
          case Light: return "Light";
          case Active: return "Active";
          default: return "Unknown";
        }
    }

    inline char const* typeToStr(Type type) {
        switch(type) {
          case REPEAT: return "REPEAT";
          case DELAY: return "DELAY";
          case DEMAND: return "DEMAND";
          case TICK: return "TICK";
          case ONCE: return "ONCE";
          case INTERR: return "INTERR";
          default: return "Unknown";
        }
    }

    bool isStarted();
    void setStarted(bool value);

    // Время от старта планировщика. На ESP32 без SNTP gettimeofday отсчитывает от boot.
    inline int64_t rts_us() {
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        return (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
    }
    inline int64_t rts_ms() { return rts_us() / 1000ULL; }

    // === Общие globals планировщика (inline-определения, C++17) ===
    // inline даёт один общий символ на все единицы трансляции — можно определять прямо в header.
    inline std::vector<Task *> coreTasks[SOC_CPU_CORES_NUM];
    inline TaskHandle_t coreTaskHandlers[SOC_CPU_CORES_NUM];
    inline std::vector<std::function<void(SleepMode)>> beforeEnterSleepCallback[SOC_CPU_CORES_NUM];
    inline std::vector<std::function<void(SleepMode)>> afterWakeUpCallback[SOC_CPU_CORES_NUM];

    inline RTC_DATA_ATTR uint64_t deepTasksTime[SOC_CPU_CORES_NUM][DEEP_TASKS_STACK];
    inline RTC_DATA_ATTR uint64_t rtsStartUs = 0;
    inline uint64_t deepTasksTimeFast[SOC_CPU_CORES_NUM][DEEP_TASKS_STACK];
    inline bool timerIsRunning = false;
    inline SleepMode sleepModeOverride = SleepMode::None;

    inline bool tickTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    inline bool lightTasksExists[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    inline bool coreSleepReady[SOC_CPU_CORES_NUM] = INIT_ARRAY(false, SOC_CPU_CORES_NUM);
    inline uint64_t minSleepTime[SOC_CPU_CORES_NUM] = INIT_ARRAY(UINT64_MAX, SOC_CPU_CORES_NUM);

    inline std::atomic<int> activeTasksCount = 0;
    inline uint64_t rtcBoot = 0;

    // Ручной режим сна — ограничение максимально допустимого уровня (используется шедулером).
    inline void setSleepMode(SleepMode mode) { sleepModeOverride = mode; }
    inline SleepMode getSleepMode() { return sleepModeOverride; }

    // === Глобальные параметры прерываний (бывш. Interrupt) ===
    inline SleepMode interruptSleepMode = SleepMode::None;
    inline RTC_DATA_ATTR bool deepInterruptsRevert = false;
    inline RTC_DATA_ATTR int deepInterruptsMode = 0;
    inline std::vector<Interrupt *> globalInterruptParams;

    inline SleepMode getInterruptSleepMode() { return interruptSleepMode; }
    inline void setInterruptSleepMode(SleepMode sleepMode) { interruptSleepMode = sleepMode; }

    inline bool getDeepInterruptsRevert() { return deepInterruptsRevert; }
    inline void setDeepInterruptsRevert(bool revert) { deepInterruptsRevert = revert; }

    inline int getDeepInterruptsMode() { return deepInterruptsMode; }
    inline void setDeepInterruptsMode(int mode) { deepInterruptsMode = mode; }

    inline const std::vector<Interrupt *> & getGlobalInterruptParams() { return globalInterruptParams; }
    inline void addGlobalInterruptParam(Interrupt * p) {
        if (!p) return;
        if (std::find(globalInterruptParams.begin(), globalInterruptParams.end(), p) == globalInterruptParams.end()) {
            globalInterruptParams.push_back(p);
        }
    }
    inline void removeGlobalInterruptParam(Interrupt * p) {
        auto it = std::find(globalInterruptParams.begin(), globalInterruptParams.end(), p);
        if (it != globalInterruptParams.end()) {
            globalInterruptParams.erase(it);
        }
    }
}
