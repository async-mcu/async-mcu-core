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

namespace async {

    int64_t rts_us();
    int64_t rts_ms();
    void checkDeepSleepTaskCanBeAdded();
    std::vector<Task *> getCoreTasks(Core core);
    TaskHandle_t getCoreTaskHandler(Core core);
    void setManualSleepMode(SleepMode level);
    SleepMode getManualSleepMode();

    void beforeEnterSleep(std::function<void(SleepMode)> callback);
    void afterWakeUp(std::function<void(SleepMode)> callback);

    void callTaskExecute(void *arg);

    Task * onDelay(SleepMode sleepMode, Duration * delay, Core core, std::function<void(Task &)> callback);

    /**
     * @brief Schedule a task to execute after a specified delay.
     * 
     * Creates a delay task that will execute once after the specified delay.
     * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
     * 
     * @tparam mode The execution mode (Active, Light, or Deep).
     * @param delay The duration to wait before executing the task.
     * @param core The CPU core to run the task on.
     * @param callback The function to execute when the delay expires.
     * @return Task& Reference to the created task.
     */
    template<SleepMode sleepMode>
    Task * onDelay(Duration * delay, Core core, std::function<void(Task &)> callback) {
        return onDelay(sleepMode, delay, core, callback);
    }

    /**
     * @brief Schedule a task to execute after a specified delay on the current core.
     * 
     * Creates a delay task that will execute once after the specified delay on the current CPU core.
     * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
     * 
     * @tparam mode The execution mode (Active, Light, or Deep).
     * @param delay The duration to wait before executing the task.
     * @param callback The function to execute when the delay expires.
     * @return Task& Reference to the created task.
     */
    template<SleepMode sleepMode>
    Task * onDelay(Duration * delay, std::function<void(Task &)> callback) {
        return onDelay<sleepMode>(delay, CURRENT_CORE, callback);
    }

    Task * onRepeat(SleepMode sleepMode, Duration * interval, Duration * startDelay, Core core, std::function<void(Task &)> callback);

    /**
     * @brief Schedule a task to execute repeatedly with a specified interval and initial delay.
     * 
     * Creates a repeating task that will first execute after startDelay, then repeat every interval.
     * The mode template parameter determines how the task is handled (Active, Light, or Deep sleep).
     * 
     * @tparam mode The execution mode (Active, Light, or Deep).
     * @param interval The interval between executions.
     * @param startDelay The initial delay before first execution.
     * @param core The CPU core to run the task on.
     * @param callback The function to execute at each interval.
     * @return Task& Reference to the created task.
     */
    template<SleepMode sleepMode> 
    Task * onRepeat(Duration * interval, Duration * startDelay, Core core, std::function<void(Task &)> callback) {
        return onRepeat(sleepMode, interval, startDelay, core, callback);
    }

    template<SleepMode sleepMode> 
    Task * onRepeat(Duration * interval, Duration * startDelay, std::function<void(Task &)> callback) {
        return onRepeat(sleepMode, interval, startDelay, CURRENT_CORE, callback);
    }

    /**
     * @brief Schedule a task to execute repeatedly with a specified interval.
     * 
     * Creates a repeating task that will execute every interval period on the specified core.
     * The first execution happens after the interval duration.
     * 
     * @tparam mode The execution mode (Active, Light, or Deep).
     * @param interval The interval between executions (also used as start delay).
     * @param core The CPU core to run the task on.
     * @param callback The function to execute at each interval.
     * @return Task& Reference to the created task.
     */
    template<SleepMode mode> 
    Task * onRepeat(Duration * interval, Core core, std::function<void(Task &)> callback) {
        return onRepeat<mode>(interval, interval, core, callback);
    }

    /**
     * @brief Schedule a task to execute repeatedly with a specified interval on the current core.
     * 
     * Creates a repeating task that will execute every interval period on the current CPU core.
     * The first execution happens after the interval duration.
     * 
     * @tparam mode The execution mode (Active, Light, or Deep).
     * @param interval The interval between executions (also used as start delay).
     * @param callback The function to execute at each interval.
     * @return Task& Reference to the created task.
     */
    template<SleepMode sleepMode> 
    Task * onRepeat(Duration * interval, std::function<void(Task &)> callback) {
        return onRepeat<sleepMode>(interval, interval, CURRENT_CORE, callback);
    }

    Task * onDemand(Core core, std::function<void(Task &)> callback);
    Task * onDemand(std::function<void(Task &)> callback);

    /**
     * @brief Schedule a task to execute exactly once.
     * 
     * Creates a task that will execute only once and then be automatically deleted.
     * The task is added to the task list and will be executed in the next cycle.
     * 
     * @param core The CPU core to run the task on.
     * @param callback The function to execute once.
     * @return Task& Reference to the created task.
     */
    Task * onOnce(Core core, std::function<void(Task &)> callback);
    Task * onOnce(std::function<void(Task &)> callback);
    Task * onOnce(Core core, Task * task);

    Task * onInit(Core core, std::function<void(Task &)> callback);


    /**
     * @brief Create a tick task that executes every cycle.
     * 
     * Creates a task that will execute on every iteration of the executor's main loop.
     * Tick tasks have the highest priority and run continuously.
     * 
     * @param core The CPU core to run the task on.
     * @param callback The function to execute on each tick.
     * @return Task& Reference to the created task.
     */
    Task * onTick(Core core, std::function<void(Task &)> callback);

    Task * onTick(std::function<void(Task &)> callback);

    void mainLoop(void * parameter);
    void initAsync();

    void startAsync();
}