// Debugging — пример для проверки логирования фреймворка:
//   1) вывод всех системных TAG из <async/Logging.h>;
//   2) уровни логгирования E/W/I/D/V и runtime-фильтрация esp_log_level_set();
//   3) периодическая диагностика планировщика (куча/стек/задачи по ядрам).
//
// <async/Logging.h> включён первым намеренно: его LOG_LOCAL_LEVEL/ESP_LOG_MAXIMUM_LEVEL
// должны вступить в силу до первого включения esp_log.h, иначе ESP_LOGD/ESP_LOGV
// вырежутся на этапе компиляции.
#include <async/Logging.h>
#include <Async.h>
#include <esp_arduino_version.h>
#include <esp_idf_version.h>

using namespace async;

static const char * TAG_MAIN = "MAIN";
static const char * TAG_DEMO = "DEMO";

// Все системные TAG фреймворка — include/async/Logging.h
struct SystemTag {
    const char * tag;   // строка-префикс в логе
    const char * name;  // имя макроса в Logging.h
};

static const SystemTag systemTags[] = {
    { TAG_EXECUTOR,    "TAG_EXECUTOR"    },
    { TAG_PIN,         "TAG_PIN"         },
    { TAG_TASK,        "TAG_TASK"        },
    { TAG_INTERRUPT,   "TAG_INTERRUPT"   },
    { TAG_CHAIN,       "TAG_CHAIN"       },
    { TAG_HTTP_SERVER, "TAG_HTTP_SERVER" },
    { TAG_SETTING,     "TAG_SETTING"     },
};
static const int systemTagsCount = sizeof(systemTags) / sizeof(systemTags[0]);

static void setAllSystemTagsLevel(esp_log_level_t level) {
    for (int i = 0; i < systemTagsCount; i++) {
        esp_log_level_set(systemTags[i].tag, level);
    }
}

// По одной строке на каждый уровень: по исчезновению строк виден текущий порог TAG_DEMO
static void demoAllLevels() {
    ESP_LOGE(TAG_DEMO, "level demo: ERROR   (1)");
    ESP_LOGW(TAG_DEMO, "level demo: WARN    (2)");
    ESP_LOGI(TAG_DEMO, "level demo: INFO    (3)");
    ESP_LOGD(TAG_DEMO, "level demo: DEBUG   (4)");
    ESP_LOGV(TAG_DEMO, "level demo: VERBOSE (5)");
}

void setup() {
    esp_log_level_set(TAG_MAIN, ESP_LOG_VERBOSE);
    esp_log_level_set(TAG_DEMO, ESP_LOG_VERBOSE);
    setAllSystemTagsLevel(ESP_LOG_VERBOSE);

    ESP_LOGI(TAG_MAIN, "=== Debugging example ===");
    ESP_LOGI(TAG_MAIN, "Arduino-ESP32 %d.%d.%d, ESP-IDF %d.%d.%d, free heap %d",
             ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH,
             ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    initAsync();

    // 1) Системные TAG: каждая строка подписана своим префиксом
    ESP_LOGI(TAG_MAIN, "1) System TAGs from <async/Logging.h>: %d", systemTagsCount);
    for (int i = 0; i < systemTagsCount; i++) {
        ESP_LOGI(systemTags[i].tag, "system TAG %d/%d: %s -> \"%s\"",
                 i + 1, systemTagsCount, systemTags[i].name, systemTags[i].tag);
    }

    // 2) Уровни: одна и та же пачка строк при понижении порога TAG_DEMO
    ESP_LOGI(TAG_MAIN, "2) esp_log_level_set(TAG_DEMO, ...):");
    ESP_LOGI(TAG_MAIN, "threshold VERBOSE -> expect all 5 lines");
    demoAllLevels();
    ESP_LOGI(TAG_MAIN, "threshold INFO -> expect E/W/I only");
    esp_log_level_set(TAG_DEMO, ESP_LOG_INFO);
    demoAllLevels();
    ESP_LOGI(TAG_MAIN, "threshold WARN -> expect E/W only");
    esp_log_level_set(TAG_DEMO, ESP_LOG_WARN);
    demoAllLevels();
    ESP_LOGI(TAG_MAIN, "threshold ERROR -> expect E only");
    esp_log_level_set(TAG_DEMO, ESP_LOG_ERROR);
    demoAllLevels();
    ESP_LOGI(TAG_MAIN, "threshold NONE -> expect no lines");
    esp_log_level_set(TAG_DEMO, ESP_LOG_NONE);
    demoAllLevels();
    ESP_LOGI(TAG_MAIN, "restore TAG_DEMO and system TAGs to INFO");
    esp_log_level_set(TAG_DEMO, ESP_LOG_INFO);
    setAllSystemTagsLevel(ESP_LOG_INFO);

    // 3) Периодическая диагностика планировщика.
    // Active (esp_timer) держит чип бодрствующим — логи гарантированно уходят в UART;
    // при Light-сне CH340 почти не отдаёт данные (см. CLAUDE.md).
    onRepeat<Active>(Duration::ms(10000), Duration::zero(), [](Task & task) {
        ESP_LOGI(TAG_MAIN, "Time %llu ms, mem %d", rts_ms(), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

        for (int core = 0; core < SOC_CPU_CORES_NUM; core++) {
            ESP_LOGI(TAG_MAIN, "Core: %d, free stack space: %d, tasks: %d",
                     core,
                     uxTaskGetStackHighWaterMark(getCoreTaskHandler((Core) core)),
                     getCoreTasks((Core) core).size());

            for (auto t : getCoreTasks((Core) core)) {
                ESP_LOGI(TAG_MAIN, " -  sleep mode: %s, type: %s",
                         modeToStr(t->getSleepMode()), typeToStr(t->getType()));
            }
        }
    });

    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
