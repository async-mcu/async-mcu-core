// CancelMatrix — матрица тестов отмены задач: все виды задач x моменты отмены.
// Самопроверяющийся прогон: каждый кейс печатает "RES <id> PASS/FAIL (детали)",
// в конце — сводка (A3 — после сводки: снайпер-кейс потенциально валит чип,
// его результат печатается отдельно).
//
// Кейсы:
//   A-группа — onDelay<Active> (одноразовая):
//     A1 cancel задолго до выстрела       : колбэк не звать, счётчик -1 ровно
//     A2 без отмены                       : ровно одно исполнение, счётчик вернулся
//     A4 self-cancel из собственного колбэка: одно исполнение, счётчик -1 ровно
//     A5 куча: 100 x (создать+cancel)    : спад кучи (residual-утечка до фикса ЖЦ)
//     A3 cancel снайпером в момент выстрела x200 (окно #7): 0 двойных декрементов
//   B-группа — onRepeat<Active> (периодическая):
//     B1 cancel извне                     : исполнения ПРЕКРАТИЛИСЬ, счётчик -1
//     B2 self-cancel на 3-м тике          : исполнения прекратились
//     B3 куча: 100 x (создать+cancel)     : спад кучи
//   C-группа — Light (живут в coreTasks):
//     C1 onDelay<Light> + cancel          : колбэк не звать; объект ОСТАЁТСЯ в
//                                           coreTasks (known #23) — фиксируем размер
//     C2 onRepeat<Light> + cancel извне   : исполнения прекратились; размер тот же
//     C3 self-cancel на 3-м тике          : прекратились; размер тот же
//   D1 onRepeat<Deep> + cancel ДО startAsync: после старта ни одного исполнения
//   E-группа — прочие типы:
//     E1 onTick + self-cancel на 3-м тике : тики прекратились
//     E2 onOnce + cancel до исполнения    : колбэк НЕ должен выполниться
//
// Каждый кейс имеет СВОЙ счётчик исполнений (зомби из неудалённых кейсов
// не засоряют чужие измерения — урок первого базлайна 2026-11-09).
//
// Результаты: базлайн до фиксов (2026-11-09) — PASS 7 / FAIL 6 + краш A3
// (A1/B1/B2/B3 → #32, E2 → #33, A3 → #7, A5/B3 → residual ЖЦ).
// После фикс-пакета #7+#32+#33 (2026-11-09): PASS 12 / FAIL 2 — только
// residual-утечки A5/B3 (104/130 Б на отмену, объекты — класс #23) и
// A3 завершается без краша (0/200).
//
// Контракт (не тестируется — UB): указатель onDelay<Active> невалиден после
// срабатывания (#9); cancel после выстрела — не использовать.
#include <Async.h>
#include <stdarg.h>

using namespace async;
static const char *TAG = "CANCEL";

// счётчики исполнений по кейсам (индексы — по кейсу, зомби изолированы)
enum Cnt { C_A1, C_A2, C_A4, C_B1, C_B2, C_C1, C_C2, C_C3, C_E1, C_E2, C_COUNT };
static std::atomic<int> cnt[C_COUNT];
#define CNT(i) [](Task &) { cnt[i]++; }

static std::atomic<bool> cbEnd{false};
static std::atomic<bool> sniperGo{false};
static Task *volatile sniperTarget = nullptr;
static int fails = 0;
static String failList = "";

static void busyUs(uint64_t us) {
    uint64_t t0 = esp_timer_get_time();
    while ((uint64_t)(esp_timer_get_time() - t0) < us) {}
}

static void res(const char *id, bool ok, const char *fmt, ...) {
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "RES %s %s (%s)", id, ok ? "PASS" : "FAIL", buf);
    if (!ok) { fails++; failList += String(" ") + id; }
}

// снайпер: ядро 1, реагирует на cbEnd немедленным cancel (окно #7)
static void sniperTask(void *) {
    for (;;) {
        while (!sniperGo.load()) vTaskDelay(1);
        while (!cbEnd.load()) { /* spin */ }
        sniperTarget->cancel();
        sniperGo = false;
    }
}

static int counter() { return activeTasksCount.load(); }

static void groupA_basics() {
    int c0, d;

    c0 = counter(); cnt[C_A1] = 0;
    { Task *t = onDelay<Active>(Duration::ms(300), CNT(C_A1));
      t->cancel(); }
    vTaskDelay(pdMS_TO_TICKS(500));
    d = counter() - c0;
    res("A1", cnt[C_A1] == 0 && d == 0, "exec=%d delta=%d (ожид. 0/0)", cnt[C_A1].load(), d);

    c0 = counter(); cnt[C_A2] = 0;
    onDelay<Active>(Duration::ms(50), CNT(C_A2));
    vTaskDelay(pdMS_TO_TICKS(300));
    d = counter() - c0;
    res("A2", cnt[C_A2] == 1 && d == 0, "exec=%d delta=%d (ожид. 1/0)", cnt[C_A2].load(), d);

    c0 = counter(); cnt[C_A4] = 0;
    onDelay<Active>(Duration::ms(50), [](Task &t) { cnt[C_A4]++; t.cancel(); });
    vTaskDelay(pdMS_TO_TICKS(300));
    d = counter() - c0;
    res("A4", cnt[C_A4] == 1 && d == 0, "exec=%d delta=%d (ожид. 1/0)", cnt[C_A4].load(), d);

    uint32_t h0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    for (int i = 0; i < 100; i++) {
        Task *t = onDelay<Active>(Duration::ms(300), [](Task &) {});
        t->cancel();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    int drop = (int)(h0 - heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    res("A5", drop <= 2048, "heap -%d Б за 100 отмен (residual ЖЦ)", drop);
}

static void groupB() {
    int c0 = counter(); cnt[C_B1] = 0;
    { Task *t = onRepeat<Active>(Duration::ms(50), Duration::zero(), CNT(C_B1));
      vTaskDelay(pdMS_TO_TICKS(280));
      int before = cnt[C_B1].load();
      t->cancel();
      vTaskDelay(pdMS_TO_TICKS(350));
      int after = cnt[C_B1].load();
      int d = counter() - c0;
      res("B1", before >= 3 && after == before && d == 0,
          "тики до=%d после=%d delta=%d (ожид. после==до)", before, after, d); }

    cnt[C_B2] = 0;
    onRepeat<Active>(Duration::ms(50), Duration::zero(), [](Task &t) {
        if (++cnt[C_B2] >= 3) t.cancel();
    });
    vTaskDelay(pdMS_TO_TICKS(700));
    res("B2", cnt[C_B2] == 3, "тиков=%d (ожид. ровно 3)", cnt[C_B2].load());

    uint32_t h0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    for (int i = 0; i < 100; i++) {
        Task *t = onRepeat<Active>(Duration::ms(500), Duration::zero(), [](Task &) {});
        t->cancel();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    int drop = (int)(h0 - heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    res("B3", drop <= 4096, "heap -%d Б за 100 отмен", drop);
}

static void groupC() {
    size_t s0 = getCoreTasks(CORE0).size();
    cnt[C_C1] = 0;
    { Task *t = onDelay<Light>(Duration::ms(300), CNT(C_C1));
      t->cancel();
      vTaskDelay(pdMS_TO_TICKS(500)); }
    size_t s1 = getCoreTasks(CORE0).size();
    res("C1", cnt[C_C1] == 0 && s1 == s0 + 1,
        "exec=%d, размер %zu -> %zu (объект остаётся: known #23)", cnt[C_C1].load(), s0, s1);

    size_t s2 = getCoreTasks(CORE0).size();
    cnt[C_C2] = 0;
    { Task *t = onRepeat<Light>(Duration::ms(60), Duration::zero(), CNT(C_C2));
      vTaskDelay(pdMS_TO_TICKS(250));
      int before = cnt[C_C2].load();
      t->cancel();
      vTaskDelay(pdMS_TO_TICKS(300));
      int after = cnt[C_C2].load();
      size_t s3 = getCoreTasks(CORE0).size();
      res("C2", before >= 2 && after == before && s3 == s2 + 1,
          "тики до=%d после=%d, размер %zu -> %zu (#23)", before, after, s2, s3); }

    cnt[C_C3] = 0;
    onRepeat<Light>(Duration::ms(60), Duration::zero(), [](Task &t) {
        if (++cnt[C_C3] >= 3) t.cancel();
    });
    vTaskDelay(pdMS_TO_TICKS(500));
    res("C3", cnt[C_C3] == 3, "тиков=%d (ожид. ровно 3)", cnt[C_C3].load());
}

static void groupE() {
    cnt[C_E1] = 0;
    onTick([](Task &t) {
        if (++cnt[C_E1] >= 3) t.cancel();
    });
    vTaskDelay(pdMS_TO_TICKS(200));
    res("E1", cnt[C_E1] == 3, "тиков=%d (ожид. ровно 3)", cnt[C_E1].load());

    cnt[C_E2] = 0;
    { Task *t = onOnce([](Task &) { cnt[C_E2]++; });
      t->cancel();
      vTaskDelay(pdMS_TO_TICKS(200)); }
    res("E2", cnt[C_E2] == 0, "exec=%d (ожид. 0: cancel до подбора воркером)", cnt[C_E2].load());
}

static void groupA3() {
    int hits = 0;
    int c0 = counter();
    for (int i = 0; i < 200; i++) {
        cbEnd = false;
        sniperTarget = onDelay<Active>(Duration::ms(25), [](Task &) {
            busyUs(300);
            cbEnd = true;
        });
        sniperGo = true;
        while (sniperGo.load()) vTaskDelay(1);
        vTaskDelay(pdMS_TO_TICKS(3));
        if (counter() != c0) hits++;
    }
    res("A3", hits == 0, "двойных декрементов %d/200 (#7)", hits);
}

static std::atomic<int> deepExec{0};

static void orchestrator(void *) {
    vTaskDelay(pdMS_TO_TICKS(200));

    groupA_basics();
    groupB();
    groupC();
    groupE();

    vTaskDelay(pdMS_TO_TICKS(3200));           // deep-дедлайн 2000 мс давно прошёл
    res("D1", deepExec == 0, "deep исполнений=%d (ожид. 0)", deepExec.load());

    ESP_LOGI(TAG, "=========== СВОДКА (без A3): PASS %d / FAIL %d ===========", 13 - fails, fails);
    if (fails) ESP_LOGI(TAG, "FAIL-лист:%s", failList.c_str());

    groupA3();   // потенциально валит чип (UAF снайпера) — потому после сводки
    ESP_LOGI(TAG, "A3 завершён без краша — см. RES A3 выше");

    vTaskDelete(NULL);
}

void setup() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    initAsync();

    // D1: deep до старта + отмена до старта (deadline 2000 мс)
    { Task *t = onRepeat<Deep>(Duration::ms(2000), [](Task &) { deepExec++; });
      t->cancel(); }

    // жезл бодрствования: долгая Active-задача, чтобы шедулер не уснул до конца
    onDelay<Active>(Duration::ms(60000), [](Task &) {});

    xTaskCreatePinnedToCore(sniperTask, "sniper", 4096, nullptr, 2, nullptr, 1);
    xTaskCreatePinnedToCore(orchestrator, "matrix", 8192, nullptr, 2, nullptr, 0);

    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
