#include <async/Duration.h>

namespace async {

    Duration::Duration(uint64_t us) {
        valueMicros = us;
    }

    Duration::~Duration() {
    }

    void Duration::set(uint64_t us) {
        this->valueMicros = us;
    }

    Duration Duration::diff(const Duration& other) const {
        return Duration(valueMicros > other.valueMicros ? valueMicros - other.valueMicros : other.valueMicros - valueMicros);
    }

    Duration Duration::add(const Duration& other) const {
        return Duration(valueMicros + other.valueMicros);
    }

    Duration Duration::subtract(const Duration& other) const {
        return Duration(valueMicros > other.valueMicros ? valueMicros - other.valueMicros : 0);
    }

    bool Duration::after(const Duration& other) const {
        return valueMicros > other.valueMicros;
    }

    bool Duration::before(const Duration& other) const {
        return valueMicros < other.valueMicros;
    }

    uint64_t Duration::us() {
        return valueMicros;
    }

    uint64_t Duration::ms() {
        return valueMicros / 1000ULL;
    }

    uint64_t Duration::sec() {
        return ms() / 1000ULL;
    }

    Duration Duration::now() {
        return Duration(esp_timer_get_time());
    }

    Duration Duration::maximum() {
        return Duration((uint64_t)-1);
    }

    Duration Duration::zero() {
        return Duration(0);
    }

    Duration * Duration::us(uint32_t us) {
        return new Duration(us);
    }

    Duration * Duration::ms(uint32_t ms) {
        return new Duration(ms * 1000ULL);
    }

    Duration& Duration::operator=(const Duration& other) {
        if (this != &other) {
            valueMicros = other.valueMicros;
        }
        return *this;
    }

    bool Duration::operator==(const Duration& other) const {
        return valueMicros == other.valueMicros;
    }

    bool Duration::operator!=(const Duration& other) const {
        return valueMicros != other.valueMicros;
    }

    bool Duration::operator<(const Duration& other) const {
        return valueMicros < other.valueMicros;
    }

    bool Duration::operator>(const Duration& other) const {
        return valueMicros > other.valueMicros;
    }

    bool Duration::operator<=(const Duration& other) const {
        return valueMicros <= other.valueMicros;
    }

    bool Duration::operator>=(const Duration& other) const {
        return valueMicros >= other.valueMicros;
    }

    Duration Duration::operator+(const Duration& other) const {
        return Duration(valueMicros + other.valueMicros);
    }

    Duration Duration::operator-(const Duration& other) const {
        return Duration(valueMicros > other.valueMicros ? valueMicros - other.valueMicros : 0);
    }

    Duration Duration::operator*(const Duration& other) const {
        return Duration(valueMicros * other.valueMicros);
    }

    Duration Duration::operator/(const Duration& other) const {
        return other.valueMicros ? Duration(valueMicros / other.valueMicros) : Duration(0);
    }

    Duration Duration::operator*(uint64_t factor) const {
        return Duration(valueMicros * factor);
    }

    Duration Duration::operator/(uint64_t divisor) const {
        return divisor ? Duration(valueMicros / divisor) : Duration(0);
    }

    Duration Duration::operator+(uint64_t other) const {
        return Duration(valueMicros + other);
    }

    Duration Duration::operator-(uint64_t other) const {
        return Duration(valueMicros > other ? valueMicros - other : 0);
    }
}