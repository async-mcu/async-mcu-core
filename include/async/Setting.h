#pragma once
#include <async/State.h>
#include <Preferences.h>

#define SETTINGS_NAMESPACE "S"
#define RW_MODE false
#define RO_MODE true

namespace async {
    template <typename T>
    class Setting : public async::State<T> {
        static_assert(sizeof(T) == 0, "Unsupported type for Setting");
    };

    template<>
    class Setting<int> : public State<int> {
        private:
        const char * uuid;
        uint16_t uuid16;
        Preferences prefs;
        int defaultValue;

        bool start() {
            prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
            this->currValue = prefs.getInt(this->uuid, defaultValue);
            prefs.end();
            return true;
        }

        public:
        Setting (const char * uuid, uint16_t uuid16, int defaultValue) : State<int>(defaultValue) {
            this->uuid = uuid;
            this->uuid16 = uuid16;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        uint16_t getUuid16() {
            return this->uuid16;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) {
            this->set(cbCallback(get()));
        }

        void set(int value, bool  force = false) override {
            if(this->get() != value || force) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putInt(this->uuid, value);
                prefs.end();
                State<int>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<int>::set(defaultValue, true);
        }
    };

    template<>
    class Setting<float> : public State<float> {
        private:
        const char * uuid;
        uint16_t uuid16;
        Preferences prefs;
        float defaultValue;

        bool start() {
            prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
            this->currValue = prefs.getFloat(this->uuid, defaultValue);
            prefs.end();
            return true;
        }

        public:
        Setting (const char * uuid, uint16_t uuid16, float defaultValue) : State<float>(defaultValue) {
            this->uuid = uuid;
            this->uuid16 = uuid16;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        uint16_t getUuid16() {
            return this->uuid16;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) {
            this->set(cbCallback(get()));
        }

        void set(float value, bool  force = false) {
            if(this->get() != value || force) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putFloat(this->uuid, value);
                prefs.end();
                State<float>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<float>::set(defaultValue, true);
        }
    };

    template<>
    class Setting<double> : public State<double> {
        private:
        const char * uuid;
        uint16_t uuid16;
        Preferences prefs;
        double defaultValue;

        bool start() {
            prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
            this->currValue = prefs.getDouble(this->uuid, defaultValue);
            prefs.end();
            return true;
        }

        public:
        Setting (const char * uuid, uint16_t uuid16, double defaultValue) : State<double>(defaultValue) {
            this->uuid = uuid;
            this->uuid16 = uuid16;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        uint16_t getUuid16() {
            return this->uuid16;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback)  {
            this->set(cbCallback(get()));
        }

        void set(double value, bool  force = false)  {
            if(this->currValue != value || force) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putDouble(this->uuid, value);
                prefs.end();
                State<double>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<double>::set(defaultValue, true);
        }
    };

    template<>
    class Setting<bool> : public State<bool> {
        private:
        const char * uuid;
        uint16_t uuid16;
        Preferences prefs;
        bool defaultValue;

        bool start() {
            prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
            this->currValue = prefs.getBool(this->uuid, defaultValue);
            prefs.end();
            return true;
        }

        public:
        Setting (const char * uuid, uint16_t uuid16, bool defaultValue) : State<bool>(defaultValue) {
            this->uuid = uuid;
            this->uuid16 = uuid16;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        uint16_t getUuid16() {
            return this->uuid16;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback)  {
            this->set(cbCallback(get()));
        }

        void set(bool value, bool  force = false)  {
            if(this->get() != value || force) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putBool(this->uuid, value);
                prefs.end();
                State<bool>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<bool>::set(defaultValue, true);
        }
    };

    template<>
    class Setting<String> : public State<String> {
        private:
        const char * uuid;
        uint16_t uuid16;
        Preferences prefs;
        String defaultValue;

        bool start() {
            prefs.begin(SETTINGS_NAMESPACE, RO_MODE);
            this->currValue = prefs.getString(this->uuid, defaultValue);
            prefs.end();
            return true;
        }

        public:
        Setting (const char * uuid, uint16_t uuid16, String defaultValue) : State<String>(defaultValue) {
            this->uuid = uuid;
            this->uuid16 = uuid16;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        uint16_t getUuid16() {
            return this->uuid16;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback)  {
            this->set(cbCallback(get()));
        }

        void set(String value, bool  force = false)  {
            if(get() != value || force) {
                prefs.begin(SETTINGS_NAMESPACE, RW_MODE);
                prefs.putString(this->uuid, value);
                prefs.end();
                State<String>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<String>::set(defaultValue, true);
        }
    };
}