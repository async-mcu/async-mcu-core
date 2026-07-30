#include <async/Boot.h>

namespace async {
    Boot::Boot(std::function<void()> callback) {
        onInit(CURRENT_CORE, [callback = std::move(callback)](Task &) {
            callback();
        });
    }
}

// Дефолтные точки входа Arduino — объявлены weak. Пример, определивший собственные
// setup()/loop(), перекрывает их; иначе используется этот вариант: он инициализирует
// и запускает планировщик, а глобальные Boot-объекты (через onInit) регистрируют
// задачи на старте. weak висит на определении, а не в заголовке — чтобы не делать
// слабыми чужие setup()/loop() в TU, включившем <async/Boot.h>.
void __attribute__((weak)) setup() {
    async::initAsync();
    async::startAsync();
}

void __attribute__((weak)) loop() {
    vTaskDelete(NULL);
}
