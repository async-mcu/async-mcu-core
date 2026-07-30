#include <async/Task.h>
#include <async/Executor.h>

using namespace async;

Task::Task(Type type, SleepMode sleepMode, Core core, Duration * delay, Duration * interval, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), delay(delay), interval(interval), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::Task(Type type, SleepMode sleepMode, Core core, Duration * delay, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), delay(delay), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::Task(Type type, SleepMode sleepMode, Core core, std::function<void(Task &)> callback)
    : type(type), sleepMode(sleepMode), core(core), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
}

Task::~Task() {
    ESP_LOGV(TAG_TASK, "Remove task sleep mode %s, type %s!", modeToStr(sleepMode), typeToStr(type));
    // delay и interval могут указывать на один объект: перегрузки onRepeat(interval,…)
    // передают interval и как startDelay, и как interval (Executor.h). Удалить один раз.
    bool delayIsInterval = (delay != nullptr && interval == delay);
    if(delay != nullptr) {
        delete delay;
        delay = nullptr;
    }
    if(interval != nullptr && !delayIsInterval) {
        delete interval;
        interval = nullptr;
    }
    if(timer != nullptr) {
        esp_timer_delete(*timer);
        delete timer;
        timer = nullptr;
    }
}

Type Task::getType() {
    return type;
}

Duration & Task::getInterval() {
    return * interval;
}

Duration & Task::getDelay() {
    return * delay;
}

SleepMode Task::getSleepMode() {
    return sleepMode;
}

esp_timer_handle_t & Task::getTimer() {
    return * timer;
}

void Task::setTimer(esp_timer_handle_t * timer) {
    this->timer = timer;
}

uint64_t Task::getNext() {
    return next;
}

void Task::setNext(uint64_t value) {
    this->next = value;
}

void Task::execute() {
    callback(*this);
}

void Task::schedule() {
    onOnce(core, this);
}

void Task::setCertainly(bool value) {
    this->certainly = value;
}

bool Task::isCertainly() {
    return certainly;
}

void * Task::getValue() {
    return value;
}

void IRAM_ATTR Task::setValue(void * value) {
    this->value = value;
}

bool Task::isCancelled() {
    return cancelled;
}

bool Task::isCounted() {
    return counted;
}

void Task::setCounted(bool value) {
    this->counted = value;
}

void Task::cancel() {
    notifyActiveTaskCancelled(*this);

    if(timer != NULL) {
        esp_timer_delete(*timer);
        delete timer;
        timer = nullptr;
    }

    cancelled = true;
    next = UINT64_MAX;
}
