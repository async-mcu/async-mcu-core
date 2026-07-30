// State<T>: «живая» реактивность во времени + несколько подписчиков onChange.
//
// В примерах State/Simple и State/SaveState все set() делаются однократно в setup().
// Здесь значение мутирует ПО ТАЙМЕРУ: onRepeat каждые 2 с инвертирует State<bool>,
// и onChange срабатывает на каждое изменение уже в цикле планировщика — не в setup().
//
// На один State можно подписать НЕСКОЛЬКО onChange: колбэки хранятся в векторе и
// вызываются все (одним отложенным таском). Тут два подписчика:
//   • логгер     — печатает (prev, current);
//   • исполнитель — имитирует «сторону эффекта» (в реальном проекте тут был бы
//     включатель реле/LED, запрос в сеть и т.п.; в этом примере без железа — только лог).
//
// onChange — отложенный (deferred): его строки появляются в отдельном тике
// планировщика, а не синхронно внутри onRepeat.

#include <Async.h>

using namespace async;
static const char *TAG_MAIN = "MAIN";

State<bool> flag(false);

void setup() {
  esp_log_level_set(TAG_MAIN, ESP_LOG_INFO);

  initAsync();

  // Подписчик 1 — наблюдатель: логирует переход.
  flag.onChange([](bool prev, bool current) {
    ESP_LOGI(TAG_MAIN, "logger:   prev %d -> current %d", prev, current);
  });

  // Подписчик 2 — исполнитель: реагирует на новое значение (сторона эффекта).
  // Оба подписчика сработают на каждое изменение flag.
  flag.onChange([](bool /*prev*/, bool current) {
    ESP_LOGI(TAG_MAIN, "executor: action for state=%d", current);
  });

  // Каждые 2 с инвертируем флаг. get() -> set(bool) всегда триггерит onChange.
  // (set(лямбда) для State<bool> дал бы ambiguous: беззахватная лямбда сводится и к
  // std::function<bool(bool)>, и к bool через function-pointer→bool.)
  onRepeat<Active>(Duration::ms(2000), [](Task &) {
    flag.set(!flag.get());
  });

  ESP_LOGI(TAG_MAIN, "Reactive running: flag toggles every 2 s (logs only, no hardware)");

  startAsync();
}

void loop() {
  vTaskDelete(NULL);
}
