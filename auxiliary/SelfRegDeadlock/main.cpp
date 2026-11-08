// SelfRegDeadlock — репро issues.md #22: self-регистрация в worker'е при полной
// очереди coreIncomingQueue (ёмкость 32).
//
// Механика: колбэк, исполняемый воркером CORE0, регистрирует 40 onOnce на
// СВОЁ ЖЕ ядро. Первые 32 xQueueSend заполняют очередь; 33-й блокируется
// навсегда (portMAX_DELAY) — единственный потребитель очереди и есть этот
// заблокированный воркер.
//
// Наблюдатели (независимы от воркеров):
//   - сердцебиение onRepeat<Active> (контекст esp_timer-задачи) каждые 500 мс;
//   - сырая задача-монитор на CORE1: статус каждые 700 мс + вердикт на 7-й с.
// Вердикт ДО фикса (2026-11-09): "reg 32" — последняя строка бомбы, LOOP DONE нет,
// spawned=0/40, сердцебиение живёт (чип не завис целиком — завис планировщик CORE0).
// ПОСЛЕ фикса #22 (прямой push_back при self-регистрации): LOOP DONE на 214-й мс,
// spawned=40/40, вердикт OK.
#include <Async.h>

using namespace async;
static const char *TAG = "DLOCK";

static std::atomic<int> spawned{0};
static std::atomic<bool> loopDone{false};
static std::atomic<int> lastReg{-1};

static void monitor(void *) {
    vTaskDelay(pdMS_TO_TICKS(7000));
    ESP_LOGI(TAG, "=========== ВЕРДИКТ ===========");
    if (loopDone.load()) {
        ESP_LOGI(TAG, "OK: self-регистрация не дедлокит (loopDone=1, spawned=%d/40)",
                 spawned.load());
    } else {
        ESP_LOGI(TAG, "ДЕДЛОК #22: бомба остановилась на reg %d, LOOP DONE нет, spawned=%d/40",
                 lastReg.load(), spawned.load());
    }
    for (;;) { vTaskDelay(pdMS_TO_TICKS(1000)); }   // не мешаем дальнейшим регрессиям
}

void setup() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    initAsync();

    // сердцебиение из контекста esp_timer (не воркер): живёт даже при дедлоке воркера
    onRepeat<Active>(Duration::ms(500), [](Task &) { ESP_LOGI(TAG, "hb"); });

    // бомба: колбэк ИСПОЛНЯЕТСЯ воркером CORE0 (certainly-подбор) и регистрирует
    // 40 onOnce на CORE0 — свой родной контекст
    onOnce(CORE0, [](Task &) {
        for (int i = 0; i < 40; i++) {
            lastReg = i;
            ESP_LOGI(TAG, "reg %d", i);
            onOnce(CORE0, [i](Task &) {
                spawned++;
                ESP_LOGI(TAG, "spawned %d", i);
            });
        }
        loopDone = true;
        ESP_LOGI(TAG, "LOOP DONE");
    });

    xTaskCreatePinnedToCore(monitor, "mon", 4096, nullptr, 1, nullptr, 1);

    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
