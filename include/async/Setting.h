#pragma once

#include <async/Executor.h>
#include <async/State.h>
#include <async/Uuid.h>
#include <Preferences.h>

#define SETTINGS_NAMESPACE "S"
#define RW_MODE false
#define RO_MODE true

static int settingNumber = 0;

namespace async {
    template <typename T>
    class Setting : public async::State<T> {
        static_assert(sizeof(T) == 0, "Unsupported type for Setting");
    };

    template<>
    class Setting<int> : public State<int> {
        private:
        Preferences prefs;
        int defaultValue;
        int settingPosition;

        public:
        Setting (int defaultValue) : State<int>(defaultValue), defaultValue(defaultValue) {}

        void init() {
            settingPosition = settingNumber++;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getInt((char *)settingPosition, defaultValue);
                prefs.end();
            });

            onChange([this](int prev, int current) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putInt((char *)settingPosition, prev);
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
        int settingPosition;

        public:
        Setting (float defaultValue) : State<float>(defaultValue), defaultValue(defaultValue) {}

        void init() {
            settingPosition = settingNumber++;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getFloat((char *)settingPosition, defaultValue);
                prefs.end();
            });

            onChange([this](float prev, float current) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putFloat((char *)settingPosition, prev);
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
        int settingPosition;

        public:
        Setting (double defaultValue) : State<double>(defaultValue), defaultValue(defaultValue) {}

        void init() {
            settingPosition = settingNumber++;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getDouble((char *)settingPosition, defaultValue);
                prefs.end();
            });

            onChange([this](double prev, double current) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putDouble((char *)settingPosition, prev);
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
        int settingPosition;

        public:
        Setting (bool defaultValue) : State<bool>(defaultValue), defaultValue(defaultValue) {}

        void init() {
            settingPosition = settingNumber++;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getDouble((char *)settingPosition, defaultValue);
                prefs.end();
            });

            onChange([this](bool prev, bool current) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putBool((char *)settingPosition, prev);
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
        int settingPosition;

        public:
        Setting (String defaultValue) : State<String>(defaultValue), defaultValue(defaultValue) {}

        void init() {
            settingPosition = settingNumber++;

            onInit(CURRENT_CORE, [this](Task &) {
                prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
                this->currValue = prefs.getString((char *)settingPosition, defaultValue);
                prefs.end();
            });

            onChange([this](String prev, String current) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putString((char *)settingPosition, prev);
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