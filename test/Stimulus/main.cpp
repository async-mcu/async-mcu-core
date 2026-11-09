// Stimulus — генератор тестовых сигналов на ВТОРОМ МК (плата-стимулятор).
// Прошивается на отдельную плату, управляется по USB-serial (115200, строки с
// завершением \n) хост-харнессом tools/stimctl.py. Дёргает пины платы-приёмника
// (DUT, с examples/Pin/*) по проводам — в том числе пока DUT спит в deep/light
// sleep (чего DUT сам себе сделать не может).
//
// Карта каналов (пин стимулятора -> пин DUT):
//   Плата-стимулятор ESP32-C3 (DevKitM-1 / SuperMini и т.п.):
//     1: GPIO3  -> GPIO14   (ActiveInterrupt / LightSleep / DeepSleepPullDown / Flash)
//     2: GPIO4  -> GPIO19   (ActiveInterrupt / LightSleep)
//     3: GPIO5  -> GPIO26   (DeepSleepInterruptPullUp)
//     4: GPIO6  -> GPIO13   (DeepSleepInterruptPullUp / MultiSleep)
//     5: GPIO7  -> GPIO15   (MultiSleep)
//     6: GPIO10 -> GPIO27   (DeepSleepInterruptPullDown)
//   Выбраны пины 3-7,10: свободны от strap (2/8/9), USB (18/19) и консоли UART0
//   (20/21). На классическом ESP32 (если стимулятор — он) карта другая, см. ниже.
// Плюс ОБЯЗАТЕЛЬНО общий GND с DUT. Обе платы — 3.3V-логика, прямое соединение.
//
// Все каналы стартуют как входы (hi-Z) — во время прошивки/загрузки DUT никто
// ничего не дёргает. Управление начинается только после явного EN.
//
// Протокол (строка -> ответ "OK ..." / "ERR ..."):
//   PING                          -> PONG
//   EN <ch>|ALL                   выход (берём пин под контроль)
//   DIS <ch>|ALL                  hi-Z
//   SET <ch> <0|1>                уровень
//   PULSE <ch> <0|1> <ms>         одиночный импульс заданной полярности
//   WAVE <ch> <half_ms> <count>   меандр (count=0 — бесконечно)
//   BOUNCE <ch> <0|1> <n> <gap>   «грязный» фронт: n переключений через gap мс
//                                  (эмуляция дребезга кнопки для охоты на окно #3)
//   STOP <ch>|ALL                 остановить WAVE/BOUNCE, уровень остаётся
//   FREQ <ch> <ms>                частотомер: пин канала -> вход (hi-Z), подсчёт
//                                  rising-фронтов за окно; ответ вида
//                                  "FREQ ch4: 440 Hz (44 edges / 100 ms, lvl 0..1)"
//   DUTY <ch> <samples>           скважность: пин -> вход, N замеров уровня;
//                                  "DUTY ch4: 25% (12503/50000, lvl 0..1)" —
//                                  различает digitalWrite 0/1 и analogWrite duty
//   STATE                         состояние каналов

#include <Arduino.h>
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3: свободные пины без strap/USB/UART (см. карту выше)
static const int CH_PINS[] = {3, 4, 5, 6, 7, 10};
#else
// Классический ESP32: RTC-пины, strap-пины не трогаем
static const int CH_PINS[] = {25, 33, 32, 4, 16, 17};
#endif
static const int CH_COUNT = sizeof(CH_PINS) / sizeof(CH_PINS[0]);

struct Channel {
    bool enabled = false;   // pinMode OUTPUT
    int level = 0;
    // WAVE
    uint32_t halfMs = 0;
    uint32_t waveCount = 0; // 0 = бесконечно
    uint32_t lastToggleMs = 0;
    // BOUNCE
    uint32_t bounceLeft = 0;
    uint32_t bounceGapMs = 0;
};

static Channel ch[CH_COUNT];

static void applyLevel(int i, int lvl) {
    ch[i].level = lvl ? 1 : 0;
    if (ch[i].enabled) digitalWrite(CH_PINS[i], ch[i].level);
}

static bool parseCh(const char * s, int & out) {
    if (!strcmp(s, "ALL")) { out = -1; return true; }
    int v = atoi(s);
    if (v < 1 || v > CH_COUNT) return false;
    out = v - 1;
    return true;
}

static void exec(String line) {
    line.trim();
    if (line.length() == 0) return;
    String cmd[6];
    int n = 0;
    int pos = 0;
    while (n < 6) {
        int sp = line.indexOf(' ', pos);
        if (sp < 0) { cmd[n++] = line.substring(pos); break; }
        cmd[n++] = line.substring(pos, sp);
        pos = sp + 1;
    }

    if (cmd[0] == "PING") { Serial.println("PONG"); return; }

    int c = -2;
    bool chOk = n >= 2 && parseCh(cmd[1].c_str(), c);

    if (cmd[0] == "EN" && chOk) {
        for (int i = 0; i < CH_COUNT; i++) {
            if (c == -1 || i == c) {
                ch[i].enabled = true;
                pinMode(CH_PINS[i], OUTPUT);
                digitalWrite(CH_PINS[i], ch[i].level);
            }
        }
        Serial.println("OK EN");
    } else if (cmd[0] == "DIS" && chOk) {
        for (int i = 0; i < CH_COUNT; i++) {
            if (c == -1 || i == c) { ch[i].enabled = false; ch[i].halfMs = 0; ch[i].bounceLeft = 0; pinMode(CH_PINS[i], INPUT); }
        }
        Serial.println("OK DIS");
    } else if (cmd[0] == "SET" && chOk && n >= 3) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        applyLevel(c, cmd[2].toInt());
        Serial.println("OK SET");
    } else if (cmd[0] == "PULSE" && chOk && n >= 4) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        int lvl = cmd[2].toInt();
        uint32_t ms = (uint32_t) cmd[3].toInt();
        applyLevel(c, !lvl);              // предуровень
        delay(ms > 5 ? 5 : ms);           // устоявшийся предуровень
        uint32_t t0 = millis();
        applyLevel(c, lvl);               // фронт
        while (millis() - t0 < ms) delay(1);
        applyLevel(c, !lvl);              // возврат (обратный фронт)
        Serial.println("OK PULSE");
    } else if (cmd[0] == "WAVE" && chOk && n >= 4) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        ch[c].halfMs = (uint32_t) cmd[2].toInt();
        ch[c].waveCount = (uint32_t) cmd[3].toInt();
        ch[c].lastToggleMs = millis();
        Serial.println("OK WAVE");
    } else if (cmd[0] == "BOUNCE" && chOk && n >= 5) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        applyLevel(c, !cmd[2].toInt());   // предуровень до серии
        ch[c].bounceLeft = (uint32_t) cmd[3].toInt();
        ch[c].bounceGapMs = (uint32_t) cmd[4].toInt();
        ch[c].lastToggleMs = millis();
        Serial.println("OK BOUNCE");
    } else if (cmd[0] == "STOP" && chOk) {
        for (int i = 0; i < CH_COUNT; i++) {
            if (c == -1 || i == c) { ch[i].halfMs = 0; ch[i].bounceLeft = 0; }
        }
        Serial.println("OK STOP");
    } else if (cmd[0] == "FREQ" && chOk && n >= 3) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        // Пин канала — вход (hi-Z), чтобы не бороться с выходом DUT
        ch[c].enabled = false;
        ch[c].halfMs = 0;
        ch[c].bounceLeft = 0;
        pinMode(CH_PINS[c], INPUT);
        uint32_t ms = (uint32_t) cmd[2].toInt();
        if (ms < 50) ms = 50;
        if (ms > 2000) ms = 2000;
        uint32_t edges = 0;
        int last = digitalRead(CH_PINS[c]);
        int lvlMin = last, lvlMax = last;
        uint32_t t0 = millis();
        while (millis() - t0 < ms) {
            int v = digitalRead(CH_PINS[c]);
            if (v != last) {
                if (v == HIGH && last == LOW) edges++;
                last = v;
            }
            if (v < lvlMin) lvlMin = v;
            if (v > lvlMax) lvlMax = v;
        }
        Serial.printf("FREQ ch%d: %lu Hz (%lu edges / %lu ms, lvl %d..%d)\r\n",
                      c + 1, edges * 1000UL / ms, edges, ms, lvlMin, lvlMax);
        Serial.println("OK FREQ");
    } else if (cmd[0] == "DUTY" && chOk && n >= 3) {
        if (c == -1) { Serial.println("ERR ch"); return; }
        ch[c].enabled = false;
        ch[c].halfMs = 0;
        ch[c].bounceLeft = 0;
        pinMode(CH_PINS[c], INPUT);
        uint32_t samples = (uint32_t) cmd[2].toInt();
        if (samples < 100) samples = 100;
        if (samples > 200000) samples = 200000;
        uint32_t high = 0;
        int lvlMin = 1, lvlMax = 0;
        for (uint32_t i = 0; i < samples; i++) {
            int v = digitalRead(CH_PINS[c]);
            if (v == HIGH) high++;
            if (v < lvlMin) lvlMin = v;
            if (v > lvlMax) lvlMax = v;
        }
        Serial.printf("DUTY ch%d: %lu%% (%lu/%lu, lvl %d..%d)\r\n",
                      c + 1, high * 100UL / samples, high, samples, lvlMin, lvlMax);
        Serial.println("OK DUTY");
    } else if (cmd[0] == "STATE") {
        for (int i = 0; i < CH_COUNT; i++) {
            Serial.printf("ch%d pin%d en%d lvl%d wave%u left%u\r\n",
                          i + 1, CH_PINS[i], ch[i].enabled, ch[i].level, ch[i].halfMs, ch[i].bounceLeft);
        }
        Serial.println("OK STATE");
    } else {
        Serial.println("ERR cmd");
    }
}

void setup() {
    Serial.begin(115200);
    for (int i = 0; i < CH_COUNT; i++) pinMode(CH_PINS[i], INPUT);  // hi-Z до EN
    Serial.print("Stimulus ready (target " CONFIG_IDF_TARGET "). Channels:");
    for (int i = 0; i < CH_COUNT; i++) Serial.printf(" %d:%d", i + 1, CH_PINS[i]);
    Serial.println();
}

void loop() {
    while (Serial.available()) {
        static String buf;
        char c = Serial.read();
        if (c == '\n' || c == '\r') { exec(buf); buf = ""; }
        else if (buf.length() < 64) buf += c;
    }

    uint32_t now = millis();
    for (int i = 0; i < CH_COUNT; i++) {
        if (!ch[i].enabled) continue;
        if (ch[i].bounceLeft > 0 && now - ch[i].lastToggleMs >= ch[i].bounceGapMs) {
            ch[i].lastToggleMs = now;
            ch[i].bounceLeft--;
            applyLevel(i, !ch[i].level);
            if (ch[i].bounceLeft == 0) ch[i].halfMs = 0;
        } else if (ch[i].halfMs > 0 && now - ch[i].lastToggleMs >= ch[i].halfMs) {
            ch[i].lastToggleMs = now;
            applyLevel(i, !ch[i].level);
            if (ch[i].waveCount > 0 && --ch[i].waveCount == 0) ch[i].halfMs = 0;
        }
    }
}
