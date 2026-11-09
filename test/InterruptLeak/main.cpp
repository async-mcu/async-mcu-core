// InterruptLeak — репро issues.md #12: утечка объектов Interrupt.
//
// Три фазы по N итераций с замером кучи:
//   A) Interrupt::cancel() без delete — главный путь утечки (#12): объект и
//      его std::function не освобождаются;
//   B) явный delete (контроль) — работает и сегодня (~Interrupt ->
//      removeInterrupt);
//   C) уничтожение Pin с живыми прерываниями (delete pin) — без ~Pin()
//      утекают все Interrupt пина; ПОСЛЕ фикса останется только residual:
//      onDemand-задача interruptTask (класс #23 — см. комментарий в ~Pin).
//
// Пины 13/15 — каналы 4/5 стенда (hi-Z, фронтов нет), режим Active удерживает
// чип бодрствующим.
//
// Результаты (200/200/50 итераций, с прогревом — разовые ~4,6 КБ на irqQueue и
// стек irq-задачи не должны попадать в замер):
//   ДО фикса #12 (2026-11-09):  A=71 Б/ит, B=0, C=272 Б/ит.
//   ПОСЛЕ (cancel -> delete this + ~Pin): A=0 Б/ит, B=0, C=170 Б/ит
//   (остаток C — onDemand/onInit-задачи пина, класс #23, см. комментарий в ~Pin).
#include <Async.h>

using namespace async;
static const char *TAG = "ILEAK";

static Pin p13(13, INPUT_PULLDOWN);

static void caseCancel() {
    Interrupt *i = p13.addInterrupt<Active>(FALLING, [](Interrupt &) {});
    i->cancel();
}

static void caseDelete() {
    Interrupt *i = p13.addInterrupt<Active>(FALLING, [](Interrupt &) {});
    delete i;
}

static void casePinDtor() {
    Pin *p = new Pin(15, INPUT_PULLDOWN);
    p->addInterrupt<Active>(FALLING, [](Interrupt &) {});
    p->addInterrupt<Active>(RISING, [](Interrupt &) {});
    delete p;
}

static void phase(const char *name, int iters, void (*fn)()) {
    for (int i = 0; i < 5; i++) fn();                  // прогрев: разовые аллокации
    vTaskDelay(pdMS_TO_TICKS(100));                    // (irqQueue + стек irq-задачи)
    uint32_t h0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    for (int i = 0; i < iters; i++) fn();
    vTaskDelay(pdMS_TO_TICKS(100));
    int drop = (int)(h0 - heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "%s: -%d Б за %d итераций = %d Б/ит", name, drop, iters, drop / iters);
}

static void orchestrator(void *) {
    vTaskDelay(pdMS_TO_TICKS(300));
    phase("A cancel()      ", 200, caseCancel);
    phase("B delete()      ", 200, caseDelete);
    phase("C delete Pin(x2)", 50, casePinDtor);
    ESP_LOGI(TAG, "=========== СВОДКА выше: Б/ит по фазам ===========");
    vTaskDelete(NULL);
}

void setup() {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    initAsync();
    // жезл бодрствования: до первой фазы interruptSleepMode ещё None — без
    // долгой Active-задачи шедулер уснёт на ~150-й мс
    onDelay<Active>(Duration::ms(120000), [](Task &) {});
    xTaskCreatePinnedToCore(orchestrator, "ileak", 4096, nullptr, 2, nullptr, 0);
    startAsync();
}

void loop() {
    vTaskDelete(NULL);
}
