#include <cmath>
#include <string>
#include <limits>

#include "safe_base.hpp"

template <typename FloatType>
requires std::is_floating_point_v<FloatType>
class SafeFloat;

template <typename FloatType>
requires std::is_floating_point_v<FloatType>
SafeFloat<FloatType> make_safe(FloatType value, std::source_location loc = std::source_location::current()) {
    return SafeFloat<FloatType>(value, loc);
}

template <typename FloatType>
requires std::is_floating_point_v<FloatType>
class SafeFloat {
public:
    using value_type = FloatType;

    // Default constructor
    explicit SafeFloat(value_type v = 0.0, std::source_location loc = std::source_location::current())
        : value(v), creation_location(loc) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            check_invalid_value(value, loc);
        }
    }

    // Conversion constructor from other floating point types
    template <typename OtherFloat>
    explicit SafeFloat(OtherFloat other, std::source_location loc = std::source_location::current())
        : creation_location(loc) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (std::isnan(other) || std::isinf(other)) {
                log_error("Invalid floating point value (NaN or Infinity) during conversion: " + std::to_string(other), loc);
            }
        }
        value = static_cast<value_type>(other);
    }

    // Copy constructor
    SafeFloat(const SafeFloat&) = default;

    // Arithmetic operators
    SafeFloat operator+(const SafeFloat& other) const {
        check_invalid_value(value, creation_location);
        check_invalid_value(other.value, other.creation_location);
        return SafeFloat(value + other.value, creation_location);
    }

    SafeFloat operator-(const SafeFloat& other) const {
        check_invalid_value(value, creation_location);
        check_invalid_value(other.value, other.creation_location);
        return SafeFloat(value - other.value, creation_location);
    }

    SafeFloat operator*(const SafeFloat& other) const {
        check_invalid_value(value, creation_location);
        check_invalid_value(other.value, other.creation_location);
        return SafeFloat(value * other.value, creation_location);
    }

    SafeFloat operator/(const SafeFloat& other) const {
        check_invalid_value(value, creation_location);
        check_invalid_value(other.value, other.creation_location);
        if (other.value == 0.0) {
            log_error("Division by zero detected", creation_location, other.creation_location);
        }
        return SafeFloat(value / other.value, creation_location);
    }

    // Compound assignment operators
    SafeFloat& operator+=(const SafeFloat& other) {
        *this = *this + other;
        return *this;
    }

    SafeFloat& operator-=(const SafeFloat& other) {
        *this = *this - other;
        return *this;
    }

    SafeFloat& operator*=(const SafeFloat& other) {
        *this = *this * other;
        return *this;
    }

    SafeFloat& operator/=(const SafeFloat& other) {
        *this = *this / other;
        return *this;
    }

    SafeFloat& operator++() {
        *this = *this + SafeFloat<FloatType>(1.0);
        return *this;
    }
    SafeFloat& operator++(int) {
        *this = *this + SafeFloat<FloatType>(1.0);
        return *this;
    }

    SafeFloat& operator--() {
        *this = *this - SafeFloat<FloatType>(1.0);
        return *this;
    }
    SafeFloat& operator--(int) {
        *this = *this - SafeFloat<FloatType>(1.0);
        return *this;
    }

    // Comparison operators
    bool operator==(const SafeFloat& other) const { return value == other.value; }
    bool operator!=(const SafeFloat& other) const { return value != other.value; }
    bool operator<(const SafeFloat& other) const { return value < other.value; }
    bool operator<=(const SafeFloat& other) const { return value <= other.value; }
    bool operator>(const SafeFloat& other) const { return value > other.value; }
    bool operator>=(const SafeFloat& other) const { return value >= other.value; }

public:
    value_type value;
    std::source_location creation_location;

    // Overflow and safety checkers
    static void check_invalid_value(value_type v, const std::source_location& loc) {
        if constexpr (SAFE_TYPE_CHECKS_ENABLED) {
            if (std::isnan(v) || std::isinf(v)) {
                log_error("Invalid floating point value (NaN or Infinity) detected: " + std::to_string(v), loc);
            }
        }
    }
};

// Typedefs for common floating point types
using sf32 = SafeFloat<float>;
using sf64 = SafeFloat<double>;
using sf80 = SafeFloat<long double>;
