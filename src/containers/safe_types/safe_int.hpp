#include <iostream>
#include <limits>
#include <source_location>
#include <string>
#include <type_traits>

#include "safe_base.hpp"

template <typename IntegerType>
requires std::is_integral_v<IntegerType>
class SafeInt;

template <typename IntegerType>
requires std::is_integral_v<IntegerType>
SafeInt<IntegerType> make_safe(IntegerType value, std::source_location loc = std::source_location::current()) {
    return SafeInt<IntegerType>(value, loc);
}

template <typename IntegerType>
requires std::is_integral_v<IntegerType>
class SafeInt {
public:
    using value_type = IntegerType;

    // Default constructor
    explicit SafeInt(value_type v = 0, std::source_location loc = std::source_location::current())
        : value(v), creation_location(loc) {}

    // Conversion constructor from other integral types
    template <typename OtherInt>
    explicit SafeInt(OtherInt other, std::source_location loc = std::source_location::current())
        : creation_location(loc) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (other > std::numeric_limits<value_type>::max() || other < std::numeric_limits<value_type>::min()) {
                log_error("Value out of range during conversion: " + std::to_string(other), loc);
            }
        }
        value = static_cast<value_type>(other);
    }

    template <typename OtherInt>
    explicit SafeInt(SafeInt<OtherInt> other_safe, std::source_location loc = std::source_location::current())
        : creation_location(loc) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (other_safe.value > std::numeric_limits<value_type>::max() || other_safe.value < std::numeric_limits<value_type>::min()) {
                log_error("Value out of range during conversion: " + std::to_string(other_safe.value), loc);
            }
        }
        value = static_cast<value_type>(other_safe.value);
    }

    // Copy constructor
    SafeInt(const SafeInt&) = default;

    // Arithmetic operators
    SafeInt operator+(const SafeInt& other) const {
        check_overflow_add(value, other.value, creation_location, other.creation_location);
        return SafeInt(value + other.value, creation_location);
    }

    SafeInt operator-(const SafeInt& other) const {
        check_overflow_sub(value, other.value, creation_location, other.creation_location);
        return SafeInt(value - other.value, creation_location);
    }

    SafeInt operator*(const SafeInt& other) const {
        check_overflow_mul(value, other.value, creation_location, other.creation_location);
        return SafeInt(value * other.value, creation_location);
    }

    SafeInt operator/(const SafeInt& other) const {
        check_divide_by_zero(other.value, creation_location, other.creation_location);
        return SafeInt(value / other.value, creation_location);
    }

    // Compound assignment operators
    SafeInt& operator+=(const SafeInt& other) {
        *this = *this + other;
        return *this;
    }

    SafeInt& operator-=(const SafeInt& other) {
        *this = *this - other;
        return *this;
    }

    SafeInt& operator*=(const SafeInt& other) {
        *this = *this * other;
        return *this;
    }

    SafeInt& operator/=(const SafeInt& other) {
        *this = *this / other;
        return *this;
    }

    SafeInt& operator--() {
        *this = *this - SafeInt<IntegerType>(1);
        return *this;
    }
    SafeInt& operator--(int) {
        *this = *this - SafeInt<IntegerType>(1);
        return *this;
    }
    SafeInt& operator++() {
        *this = *this + SafeInt<IntegerType>(1);
        return *this;
    }
    SafeInt& operator++(int) {
        *this = *this + SafeInt<IntegerType>(1);
        return *this;
    }
    // Comparison operators
    bool operator==(const SafeInt& other) const { return value == other.value; }
    bool operator!=(const SafeInt& other) const { return value != other.value; }
    bool operator<(const SafeInt& other) const { return value < other.value; }
    bool operator<=(const SafeInt& other) const { return value <= other.value; }
    bool operator>(const SafeInt& other) const { return value > other.value; }
    bool operator>=(const SafeInt& other) const { return value >= other.value; }

public:
    value_type value;
    std::source_location creation_location;

    // Overflow and safety checkers
    static void check_overflow_add(value_type a, value_type b, const std::source_location& loc1, const std::source_location& loc2) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if ((b > 0 && a > std::numeric_limits<value_type>::max() - b) ||
                (b < 0 && a < std::numeric_limits<value_type>::min() - b)) {
                log_error("Addition overflow: " + std::to_string(a) + " + " + std::to_string(b), loc1, loc2);
            }
        }
    }

    static void check_overflow_sub(value_type a, value_type b, const std::source_location& loc1, const std::source_location& loc2) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if ((b > 0 && a < std::numeric_limits<value_type>::min() + b) ||
                (b < 0 && a > std::numeric_limits<value_type>::max() + b)) {
                log_error("Subtraction overflow: " + std::to_string(a) + " - " + std::to_string(b), loc1, loc2);
            }
        }
    }

    static void check_overflow_mul(value_type a, value_type b, const std::source_location& loc1, const std::source_location& loc2) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (a != 0 && b != 0 &&
                (a > std::numeric_limits<value_type>::max() / b || a < std::numeric_limits<value_type>::min() / b)) {
                log_error("Multiplication overflow: " + std::to_string(a) + " * " + std::to_string(b), loc1, loc2);
            }
        }
    }

    static void check_divide_by_zero(value_type b, const std::source_location& loc1, const std::source_location& loc2) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (b == 0) {
                log_error("Division by zero detected", loc1, loc2);
            }
        }
    }
};

// Typedefs for common integer types
using si8  = SafeInt<int8_t>;
using si16 = SafeInt<int16_t>;
using si32 = SafeInt<int32_t>;
using si64 = SafeInt<int64_t>;

using su8  = SafeInt<uint8_t>;
using su16 = SafeInt<uint16_t>;
using su32 = SafeInt<uint32_t>;
using su64 = SafeInt<uint64_t>;
