#pragma once
#include <async/State.h>
#include <Preferences.h>

namespace async {
    template <typename T>
    class Setting : public async::State<T> {
        static_assert(sizeof(T) == 0, "Unsupported type for Setting");
    };

    template<>
    class Setting<int> : public State<int> {
        private:
        const char * uuid;
        Preferences prefs;
        int defaultValue;
        bool init = false;

        void start() {
            init = true;
            this->value = prefs.getInt(this->uuid, defaultValue);
        }

        public:
        Setting (const char * uuid, int defaultValue) : State<int>(defaultValue) {
            this->uuid = uuid;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        int get() override {
            if(!init) start();
            return this->value;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) override {
            this->set(cbCallback(get()));
        }

        void set(int value, bool  force = false) override {
            if(!init) start();

            if(this->value != value || force) {
                prefs.putInt(this->uuid, value);
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
        Preferences prefs;
        float defaultValue;
        bool init = false;

        void start() {
            init = true;
            this->value = prefs.getFloat(this->uuid, defaultValue);
        }

        public:
        Setting (const char * uuid, float defaultValue) : State<float>(defaultValue) {
            this->uuid = uuid;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        float get() override {
            if(!init) start();
            return this->value;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) override {
            this->set(cbCallback(get()));
        }

        void set(float value, bool  force = false) override {
            if(!init) start();

            if(this->value != value || force) {
                prefs.putFloat(this->uuid, value);
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
        Preferences prefs;
        double defaultValue;
        bool init = false;

        void start() {
            init = true;
            this->value = prefs.getDouble(this->uuid, defaultValue);
        }

        public:
        Setting (const char * uuid, double defaultValue) : State<double>(defaultValue) {
            this->uuid = uuid;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        double get() override {
            if(!init) start();
            return this->value;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) override {
            this->set(cbCallback(get()));
        }

        void set(double value, bool  force = false) override {
            if(!init) start();

            if(this->value != value || force) {
                prefs.putDouble(this->uuid, value);
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
        Preferences prefs;
        bool defaultValue;
        bool init = false;

        void start() {
            init = true;
            this->value = prefs.getBool(this->uuid, defaultValue);
        }

        public:
        Setting (const char * uuid, bool defaultValue) : State<bool>(defaultValue) {
            this->uuid = uuid;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        bool get() override {
            if(!init) start();
            return this->value;
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) override {
            this->set(cbCallback(get()));
        }

        void set(bool value, bool  force = false) override {
            if(!init) start();

            if(this->value != value || force) {
                prefs.putBool(this->uuid, value);
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
        Preferences prefs;
        String defaultValue;
        bool init = false;

        void start() {
            init = true;

            this->value = prefs.getString(this->uuid, defaultValue);
        }

        public:
        Setting (const char * uuid, String defaultValue) : State<String>(defaultValue) {
            this->uuid = uuid;
            this->defaultValue = defaultValue;
        }

        const char * getUuid() {
            return this->uuid;
        }

        String get() override {
            if(!init) start();
            return State<String>::get();
        }

        void getAndSet(GetAndSetAllArgsCallback cbCallback) override {
            this->set(cbCallback(get()));
        }

        void set(String value, bool  force = false) override {
            if(!init) start();

            if(get() != value || force) {
                prefs.putString(this->uuid, value);
                State<String>::set(value, force);
            }
        }

        void reset() {
            prefs.remove(this->uuid);
            State<String>::set(defaultValue, true);
        }

        ~Setting() {
            if(init) {
                prefs.end();
            }
        }
    };
    // template<typename T>
    // class Setting : public State<T> {
    //     private:
    //     const char * uuid;
    //     Preferences prefs;
    //     T defaultValue;
    //     bool init = false;

    //     void start() {
    //         init = true;
    //         prefs.begin("settings", false);

    //         if (std::is_same<T, int>::value) {
    //             this->value = prefs.getInt(this->uuid, defaultValue);
    //         }
    //         else if (std::is_same<T, float>::value) {
    //             this->value = prefs.getFloat(this->uuid, defaultValue);
    //         }
    //         else if (std::is_same<T, double>::value) {
    //             this->value = prefs.getDouble(this->uuid, defaultValue);
    //         }
    //         else if (std::is_same<T, bool>::value) {
    //             this->value = prefs.getBool(this->uuid, defaultValue);
    //         }
    //         else if (std::is_same<T, String>::value) {
    //             this->value = prefs.getString(this->uuid, defaultValue);
    //         }
    //         else if (std::is_same<T, const char*>::value) {
    //             String result = prefs.getString(this->uuid, String(defaultValue));
    //             this->value = result.c_str(); // Не забудьте освободить память!
    //         }
    //         else {
    //             static_assert(sizeof(T) == -1, "Unsupported type for Setting");
    //         }
    //     }

    //     public:
    //     Setting<T> (const char * uuid, T defaultValue) : State<T>(defaultValue) {
    //         this->uuid = uuid;
    //         this->defaultValue = defaultValue;
    //     }

    //     const char * getUuid() {
    //         return this->uuid;
    //     }

    //     T get() override {
    //         if(!init) start();
    //         return this->value;
    //     }

    //     void set(T value, bool  force = false) override {
    //         if(!init) start();

    //         Serial.println("set");

    //         if(this->value != value || force) {
    //             if (std::is_same<T, int>::value) {
    //         Serial.println("set int");
    //                 prefs.putInt(this->uuid, value);
    //             }
    //             else if (std::is_same<T, float>::value) {
    //                 prefs.putFloat(this->uuid, value);
    //             }
    //             else if (std::is_same<T, double>::value) {
    //                 prefs.putDouble(this->uuid, value);
    //             }
    //             else if (std::is_same<T, bool>::value) {
    //                 prefs.putBool(this->uuid, value);
    //             }
    //             else if(std::is_same<T, String>::value) {
    //                 prefs.putString(this->uuid, value);
    //             }
    //             else {
    //                 static_assert(sizeof(T) == 0, "Unsupported type for Setting");
    //             }

    //             State<T>::set(value, force);
    //         }
    //     }
    // };
}