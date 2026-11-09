// ActiveRace — репро/регресс-тест issues.md #7 (TOCTOU двойного декремента
// activeTasksCount): снайпер на ядре 1 зовёт cancel() в момент выстрела
// onDelay<Active>.
//
// ДО фикса (2026-11-09): двойной декремент в каждую итерацию, чип уходил в
// deep sleep при живых задачах; на замусоренной куче — UAF-краш.
// ПОСЛЕ фикса #7 (claim + мьютекс финализации): 300 итераций, 0 попаданий,
// чип жив. Оставлен как регресс-тест гонки.
//
// Механика гонки: callTaskExecute (задача esp_timer, ядро 0) делает
//   execute() -> if (isCounted()) { activeTasksCount--; setCounted(false); }
// Между его проверкой counted и очисткой — атомарный декремент через
// межъядерный лок (сотни нс). cancel() с другого ядра, успевший проверить
// counted в этом зазоре, декрементирует ВТОРОЙ раз.
//
// Схема воспроизведения (каждая итерация — независимая попытка):
//   T1 = onDelay<Active>(25 мс, колбэк: работа 300 мкс; cbEnd=true ПОСЛЕДНЕЙ строкой)
//   снайпер (ядро 1, prio 2): спин до cbEnd -> немедленно t->cancel()
//     — его проверка counted попадает в зазор сразу после возврата колбэка.
// Контроль: expected счётчик = witness + 2 guard'а = 3 (T1 уже сошёл).
//   Попадание = двойной декремент: счётчик 2 (и ниже — с накоплением).
//
// Последствие (#7 -> неверное решение о сне): в конце отменяем guard'ы.
//   При живом witness счётчик станет 1-hits: <=0 -> шедулер усыпит чип
//   (deep sleep без источников) — логи замолкают навсегда до ресета.
#include <Async.h>

using namespace async;
static const char *TAG = "RACE";

static std::atomic<bool> cbEnd{false};
static Task *guard1 = nullptr;
static Task *guard2 = nullptr;
static int hits = 0;

static void busyUs(uint64_t us) {
    uint64_t t0 = esp_timer_get_time();
    while ((uint64_t)(esp_timer_get_time() - t0) < us) {}
}

static void raceTask(void *) {
    const int ITER = 300;
    vTaskDelay(pdMS_TO_TICKS(100));  // дать воркерам стартовать

    for (int i = 0; i < ITER; i++) {
        cbEnd = false;
        Task *t = onDelay<Active>(Duration::ms(25), [](Task &) {
            busyUs(300);   // работа в колбэке; флаг — последней строкой
            cbEnd = true;
        });

        uint64_t t0 = esp_timer_get_time();
        while (!cbEnd.load() && (uint64_t)(esp_timer_get_time() - t0) < 100000) {}
        t->cancel();       // гонка: чья проверка counted раньше чьей очистки

        vTaskDelay(pdMS_TO_TICKS(4));  // декременты утряслись
        int c = activeTasksCount.load();
        if (c != 3) {
            hits++;
            ESP_LOGI(TAG, "iter %d: counter=%d, expected 3 — ДВОЙНОЙ ДЕКРЕМЕНТ (#7)", i, c);
        }
    }

    ESP_LOGI(TAG, "итог: итераций %d, попаданий %d", ITER, hits);
    ESP_LOGI(TAG, "отменяю guard'ы: счётчик станет 1-%d при живом witness", hits);
    guard1->cancel();
    guard2->cancel();
    vTaskDelay(pdMS_TO_TICKS(300));  // дать логу покинуть UART до возможного сна
    ESP_LOGI(TAG, "final counter=%d (witness жив): <=0 -> шедулер сейчас усыпит чип",
             activeTasksCount.load());
    vTaskDelete(NULL);
}

void setup() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    initAsync();

    onRepeat<Active>(Duration::ms(700), [](Task &) { ESP_LOGI(TAG, "witness tick"); });
    guard1 = onDelay<Active>(Duration::ms(600000), [](Task &) {});
    guard2 = onDelay<Active>(Duration::ms(600000), [](Task &) {});
    // стартовый уровень: witness + 2 guard'а = 3

    ESP_LOGI(TAG, "race start: %d итераций (onDelay<Active> 25 мс + cancel снайпером с ядра 1)", 300);
    xTaskCreatePinnedToCore(raceTask, "race", 4096, nullptr, 2, nullptr, 1);

    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
