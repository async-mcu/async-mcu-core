# Pin / WritePin

`Pin::digitalWrite` / `Pin::analogWrite` — управление уровнями GPIO: цикл фаз по
2 с на pin13 (HIGH → LOW → ШИМ ~25% → ШИМ ~75%) и меандр-«blink» на pin15.
До 2026-11-08 пример был копией старого interrupt-каркаса (issues.md #26) —
переписан по назначению.

## ⚠️ Главный урок примера: возврат от analogWrite к digitalWrite (issues.md #29)

`analogWrite` подключает пин к LEDC **через GPIO-матрицу** (режим
`OUTPUT_ANALOG`). `noTone()` только останавливает ШИМ — пин замирает на
idle-уровне LEDC, но матрица продолжает маршрутизировать LEDC, и последующий
`digitalWrite` (`gpio_set_level`) **игнорируется**. Подтверждено измерением на
стенде: после ШИМ фаза HIGH давала 0% скважности вместо 100%.

Возврат к честному digitalWrite — только через `setMode(OUTPUT)`: он и глушит
LEDC, и переключает матрицу обратно на GPIO (см. цикл в примере).

## Отчёт о проверке

- **Дата:** 2026-11-08
- **Стенд:** DUT `esp32dev` (COM11) + стимулятор ESP32-C3 (COM12, см.
  `test/`): GPIO13/GPIO15 DUT соединены с каналами 4/5; команда `DUTY`
  (N замеров уровня на входе C3) измеряет скважность — все фазы проверены
  независимо от DUT
- **Фреймворк:** Arduino-ESP32 **3.3.5**, ESP-IDF **5.5.1** (`pioarduino#55.03.35`)
- **Результат: OK** (после исправления возврата от ШИМ через `setMode`):

| Фаза pin13 | Заявлено | Измерено C3 (30000 замеров) |
|---|---|---|
| `digitalWrite HIGH` (до ШИМ) | 100% | **100%** |
| `digitalWrite LOW` | 0% | **0%** |
| `analogWrite 256` (10 бит) | ~25% | **24%** |
| `analogWrite 768` | ~75% | **74%** |
| `digitalWrite HIGH` (после ШИМ, с `setMode(OUTPUT)`) | 100% | **100%** |
| blink pin15 (фаза HIGH) | 100% | **100%** |

- Отрицательный эксперимент (до фикса): та же фаза HIGH после ШИМ с
  `noTone()` вместо `setMode` — **0%** (пин заморожен на idle-уровне LEDC).

```
I (2133) MAIN: pin13: digitalWrite HIGH
I (4133) MAIN: pin13: digitalWrite LOW
I (6134) MAIN: pin13: analogWrite 256 (~25%)
I (8133) MAIN: pin13: analogWrite 768 (~75%)
I (10133) MAIN: pin13: digitalWrite HIGH    <- setMode(OUTPUT) вернул пин
DUTY ch4: 100% / 0% / 24% / 74% / 100%
```

## Сборка и прошивка

```bash
pio run -e pin_write -t upload
PYTHONIOENCODING=utf-8 pio device monitor -p COM11
```
