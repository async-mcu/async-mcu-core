#include <async/Executor.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
    ESP_LOGI(TAG_MAIN, "Before script time %llu ms\n", rts_ms());

    esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
    esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);
    esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

    onRepeat<Light>(Duration::ms(10000), [](Task * task) {
        ESP_LOGI(TAG_MAIN, "10 seconds passed, time %llu ms, mem %d, free stack space0: %d, free stack space1: %d, tasks0 %d, tasks1 %d \n", 
        rts_ms(), 
        heap_caps_get_free_size(MALLOC_CAP_INTERNAL), 
        uxTaskGetStackHighWaterMark(taskLoopCore0),
        uxTaskGetStackHighWaterMark(taskLoopCore1),
        tasks[0].size(),
        tasks[1].size());

        for(auto t : tasks[0]) {
            ESP_LOGI(TAG_MAIN, "  task0 mode %d type %d\n", t->getMode(), t->getType());
        } 
        for(auto t : tasks[1]) {
            ESP_LOGI(TAG_MAIN, "  task1 mode %d type %d\n", t->getMode(), t->getType());
        }
    });

    start();
};

void loop() {
    vTaskDelete(NULL);
}