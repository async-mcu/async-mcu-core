#include <Async.h>

using namespace async;
static const char* TAG_MAIN = "MAIN";

void setup() {
    ESP_LOGI(TAG_MAIN, "Before script time %llu ms\n", rts_ms());

    esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
    esp_log_level_set(TAG_PIN, ESP_LOG_VERBOSE);
    esp_log_level_set(TAG_EXECUTOR, ESP_LOG_VERBOSE);

    onRepeat<Light>(Duration::ms(10000), Duration::zero(), [](Task * task) { 
        ESP_LOGI(TAG_MAIN, "Time %llu ms, mem %d", rts_ms(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        for(int core=0; core < SOC_CPU_CORES_NUM; core++) {
            ESP_LOGI(TAG_MAIN, "Core: %d, free stack space: %d, tasks: %d", 
                core, 
                uxTaskGetStackHighWaterMark(getCoreTaskHandler((Core) core)),
                getCoreTasks((Core) core).size());
            
            for(auto t : getCoreTasks((Core) core)) {
                printf(" -  sleep mode: %s, type: %s\n", modeToStr(t->getSleepMode()), typeToStr(t->getType()));
            } 
        }


    });

    start();
};

void loop() {
    vTaskDelete(NULL);
}