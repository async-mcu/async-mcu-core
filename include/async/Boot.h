#pragma once

#include <functional>
#include <Async.h>

namespace async {

/// Глобальный объект, откладывающий колбэк до фазы инициализации планировщика.
///
/// Глобальные конструкторы выполняются ДО `initAsync()`, поэтому регистрировать
/// задачи (`onRepeat`/`onDelay`/...) прямо в глобальном объекте нельзя — планировщик
/// ещё не готов. `Boot` оборачивает колбэк в `onInit`: он выполнится, когда `initAsync()`
/// (дефолтный `setup()` из `Boot.cpp`) запустит фазу INIT.
///
/// Пример — `examples/Boot`:
/// ```cpp
/// async::Boot boot0([]() {
///     onRepeat<Deep>(Duration::ms(2000), CORE0, [](Task &) { /* ... */ });
/// });
/// ```
class Boot {
    public:
        explicit Boot(std::function<void()> callback);
};

}
