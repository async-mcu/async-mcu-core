#include <async/Task.h>
#include <async/Executor.h>

using namespace async;

Task::Task(Type type, Mode mode, Core core, Duration * delay, Duration * interval, std::function<void(Task *)> callback)
    : type(type), mode(mode), core(core), delay(delay), interval(interval), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(mode), typeToStr(type));
}

Task::Task(Type type, Mode mode, Core core, Duration * delay, std::function<void(Task *)> callback)
    : type(type), mode(mode), core(core), delay(delay), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(mode), typeToStr(type));
}

Task::Task(Type type, Mode mode, Core core, std::function<void(Task *)> callback)
    : type(type), mode(mode), core(core), callback(callback) {
    ESP_LOGV(TAG_TASK, "Create task mode %s, type %s!", modeToStr(mode), typeToStr(type));
}

Task::~Task() {
    ESP_LOGV(TAG_TASK, "Remove task mode %s, type %s!", modeToStr(mode), typeToStr(type));
    if(delay != nullptr) {
        delete delay;
        delay = nullptr;
    }   
    if(interval != nullptr) {
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

Mode Task::getMode() {
    return mode;
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
    callback(this);
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

void Task::setValue(void * value) {
    this->value = value;
}

void Task::cancel() {
    if(timer != NULL) {
        esp_timer_delete(*timer);
        delete timer;
    }

    next = UINT64_MAX;
}
