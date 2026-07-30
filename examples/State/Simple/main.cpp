// State<T>: наблюдаемое значение с колбэками onChange.
//
// Четыре способа установить значение:
//   1) set(value)              — безусловно (триггерит onChange всегда)
//   2) set(callback)           — новое значение = f(текущее)
//   3) setIfChanged(value)     — только при изменении (вернёт bool)
//   4) setIfChanged(callback)  — f(текущее), только при изменении
//
// Дополнительно тут показано:
//   • НЕСКОЛЬКО подписчиков onChange — колбэки хранятся в векторе и срабатывают
//     все на одно set() (ниже у `value` их два: логгер и наблюдатель).
//   • Nullable через State<T*>: set(nullptr) [setNull()] / isNotNull(). Удобно
//     для опционального владения ресурсом; ниже — State<int*> handle.
//
// onChange — отложенный (deferred): срабатывает в следующем цикле планировщика,
// поэтому его логи появляются ПОСЛЕ синхронных ESP_LOGI из setup().

#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

State<int> value(0);

// Nullable: State<T*> хранит указатель. setNull() = set(nullptr), isNotNull() —
// есть ли значение. Адрес статической переменной берём как «ресурс» (без new/утечек).
static int resource = 42;
State<int*> handle(nullptr);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();

  // Подписчик 1: срабатывает на каждое РЕАЛЬНОЕ изменение (deferred). setIfChanged
  // подавляет вызов, если новое значение совпадает с текущим.
  value.onChange([](int prev, int current) {
    ESP_LOGI(TAG_MAIN, "onChange: prev %d -> current %d", prev, current);
  });

  // Подписчик 2 (наблюдатель): на один set срабатывают ВСЕ подписчики — этот
  // блок отработает вместе с первым для каждого изменения value.
  value.onChange([](int /*prev*/, int current) {
    ESP_LOGI(TAG_MAIN, "observer: value is now %d", current);
  });

  // 1) set(value) — безусловная установка.
  value.set(10);                       // 0 -> 10 (изменение)

  // 2) set(callback) — новое значение вычисляется из текущего.
  value.set([](int prev) {             // 10 -> 11 (изменение)
    return prev + 1;
  });

  // 3) setIfChanged(value) — ничего не делает и НЕ триггерит onChange, если значение то же.
  //    Возвращает true, если значение изменилось.
  ESP_LOGI(TAG_MAIN, "setIfChanged(11): changed=%d", value.setIfChanged(11));  // 0 (11 == 11)
  ESP_LOGI(TAG_MAIN, "setIfChanged(42): changed=%d", value.setIfChanged(42));  // 1 (11 -> 42)

  // 4) setIfChanged(callback) — то же, но новое значение = f(текущее). Возвращает текущее.
  ESP_LOGI(TAG_MAIN, "setIfChanged(cb same): %d", value.setIfChanged([](int prev) {
    return prev;                       // 42 -> 42 (нет изменения)
  }));
  ESP_LOGI(TAG_MAIN, "setIfChanged(cb +100): %d", value.setIfChanged([](int prev) {
    return prev + 100;                 // 42 -> 142 (изменение)
  }));

  ESP_LOGI(TAG_MAIN, "final value: %d", value.get());

  // --- Nullable: State<T*> и setNull()/isNotNull() ---------------------------
  // handle меняется с/на nullptr — onChange ловит оба перехода.
  handle.onChange([](int *prev, int *current) {
    ESP_LOGI(TAG_MAIN, "handle onChange: prev %p -> current %p", prev, current);
  });

  ESP_LOGI(TAG_MAIN, "isNotNull (init): %d", handle.isNotNull());   // 0 — nullptr
  handle.set(&resource);                                            // nullptr -> &resource
  ESP_LOGI(TAG_MAIN, "isNotNull (set):  %d", handle.isNotNull());   // 1
  handle.setNull();                                                 // &resource -> nullptr
  ESP_LOGI(TAG_MAIN, "isNotNull (null): %d", handle.isNotNull());   // 0

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
