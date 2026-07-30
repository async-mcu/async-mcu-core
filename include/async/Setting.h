#pragma once

#include <async/Executor.h>
#include <async/State.h>
#include <async/Uuid.h>
#include <Preferences.h>
#include <nvs.h>

#define SETTINGS_NAMESPACE "S"
#define RW_MODE false
#define RO_MODE true

// NVS-ключ (обязательный аргумент name конструктора) ограничен 15 символами
// (NVS_KEY_NAME_MAX_SIZE-1). При превышении конструктор падает (assertNvsKeyLength):
// длинный ключ иначе молча не пишется (putX вернёт 0) — это программистская ошибка.
namespace async {
    // Fail-fast проверка длины NVS-ключа; зовётся из каждого Setting-конструктора.
    inline void assertNvsKeyLength(const char * key) {
        if ((int) strlen(key) > NVS_KEY_NAME_MAX_SIZE - 1) {
            ESP_LOGE(TAG_SETTING, "NVS key '%s' too long: %d chars, max %d (NVS_KEY_NAME_MAX_SIZE-1)",
                     key, (int) strlen(key), NVS_KEY_NAME_MAX_SIZE - 1);
            esp_system_abort("Setting: NVS key longer than 15 chars");
        }
    }

    template <typename T>
    class Setting : public async::State<T> {
        static_assert(sizeof(T) == 0, "Unsupported type for Setting");
    };

    template<>
    class Setting<int> : public State<int> {
        private:
        Preferences prefs;
        int defaultValue;
        String settingKey;

        public:
        Setting (int defaultValue, const char* key) : State<int>(defaultValue), defaultValue(defaultValue) {
            assertNvsKeyLength(key);
            settingKey = key;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getInt(settingKey.c_str(), this->defaultValue);
                prefs.end();
            });

            onChange([this](int prev, int current) {
                if (prev == current) return; // нет изменения → пропускаем запись NVS (износ flash)
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putInt(settingKey.c_str(), current);
                prefs.end();
            });
        }

        void reset() {
            set(defaultValue);
        }
    };

    template<>
    class Setting<float> : public State<float> {
        private:
        Preferences prefs;
        float defaultValue;
        String settingKey;

        public:
        Setting (float defaultValue, const char* key) : State<float>(defaultValue), defaultValue(defaultValue) {
            assertNvsKeyLength(key);
            settingKey = key;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getFloat(settingKey.c_str(), this->defaultValue);
                prefs.end();
            });

            onChange([this](float prev, float current) {
                if (prev == current) return; // нет изменения → пропускаем запись NVS (износ flash)
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putFloat(settingKey.c_str(), current);
                prefs.end();
            });
        }

        void reset() {
            set(defaultValue);
        }
    };

    template<>
    class Setting<double> : public State<double> {
        private:
        Preferences prefs;
        double defaultValue;
        String settingKey;

        public:
        Setting (double defaultValue, const char* key) : State<double>(defaultValue), defaultValue(defaultValue) {
            assertNvsKeyLength(key);
            settingKey = key;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getDouble(settingKey.c_str(), this->defaultValue);
                prefs.end();
            });

            onChange([this](double prev, double current) {
                if (prev == current) return; // нет изменения → пропускаем запись NVS (износ flash)
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putDouble(settingKey.c_str(), current);
                prefs.end();
            });
        }

        void reset() {
            set(defaultValue);
        }
    };

    template<>
    class Setting<bool> : public State<bool> {
        private:
        Preferences prefs;
        bool defaultValue;
        String settingKey;

        public:
        Setting (bool defaultValue, const char* key) : State<bool>(defaultValue), defaultValue(defaultValue) {
            assertNvsKeyLength(key);
            settingKey = key;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getBool(settingKey.c_str(), this->defaultValue);
                prefs.end();
            });

            onChange([this](bool prev, bool current) {
                if (prev == current) return; // нет изменения → пропускаем запись NVS (износ flash)
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putBool(settingKey.c_str(), current);
                prefs.end();
            });
        }

        void reset() {
            set(defaultValue);
        }
    };

    template<>
    class Setting<String> : public State<String> {
        private:
        Preferences prefs;
        String defaultValue;
        String settingKey;

        public:
        Setting (String defaultValue, const char* key) : State<String>(defaultValue), defaultValue(defaultValue) {
            assertNvsKeyLength(key);
            settingKey = key;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getString(settingKey.c_str(), this->defaultValue);
                prefs.end();
            });

            onChange([this](String prev, String current) {
                if (prev == current) return; // нет изменения → пропускаем запись NVS (износ flash)
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putString(settingKey.c_str(), current);
                prefs.end();
            });
        }

        String operator+(const Setting<String>& other) const {
            return this->currValue + other.currValue;
        }

        String operator+(const String& other) const {
            return this->currValue + other;
        }

        void reset() {
            set(defaultValue);
        }
    };
}
