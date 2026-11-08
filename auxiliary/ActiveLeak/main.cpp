// ActiveLeak — воспроизведение issues.md #9: утечка сработавших одноразовых
// Active-задач (callTaskExecute не удаляет Task/esp_timer) и каждой пары
// tone(freq, duration) (внутри — та же onDelay<Active>).
//
// Фаза A: каждые 50 мс создаём onDelay<Active>(5 мс) — сработает и (до фикса)
//         утечёт: Task + std::function + Duration + esp_timer-обёртка + структура
//         esp_timer (~сотни байт за выстрел).
// Фаза B: каждые 50 мс pin13.tone(700, 5 мс) — та же утечка на каждый вызов.
// Куча логируется каждые 20 итераций; устойчивый отрицательный наклон = утечка.
//
// Результаты: ДО фикса #9 (2026-11-09) — линейный спад 240 Б/выстрел (фаза A)
// и 173 Б/вызов tone (фаза B), 332820→250320 Б за 400 итераций; ПОСЛЕ фикса —
// куча плоская (стартовые ~13,4 КБ + один шаг LEDC −184 Б, далее 0 Б/итерацию).
#include <Async.h>

using namespace async;
static const char *TAG = "LEAK";

Pin p13(13, OUTPUT);

static uint32_t heap0 = 0;

static void report(const char *phase, int n) {
    uint32_t h = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "%s n=%3d free=%6u (delta %5d от старта)", phase, n, h, (int)(h - heap0));
}

void setup() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    initAsync();

    heap0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    ESP_LOGI(TAG, "start free=%u", heap0);

    int phase = 0, n = 0;
    onRepeat<Active>(Duration::ms(50), [phase, n](Task &) mutable {
        const char *ph = (phase < 200) ? "A(delay)" : "B(tone)";

        if (phase < 200) {
            onDelay<Active>(Duration::ms(5), [](Task &) {});   // выстрелит и утечёт (#9)
        } else {
            p13.tone(700, Duration::ms(5));                     // каждая пара — утечка (#9)
        }

        phase++; n++;
        if (n % 20 == 0) report(ph, n);
        if (n == 200 || n == 400) {
            ESP_LOGI(TAG, "=== фаза %s завершена ===", ph);
        }
    });

    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
