#include "safe_int.hpp"
#include "safe_float.hpp"

// all casts HAVE to be explicit. I am not sure this can be avoided
int main() {
    si32 a(100);
    si32 b(200);

    si32 c = a + b;              // Addition
    a += b;                      // Compound addition
    si64 d = si64(si16(a * b));  // Multiplication

    su32 ua = su32(0);
    ua --; // underflow

    si32 e = si32(std::numeric_limits<si32::value_type>::max()-1) + make_safe(1);  // Overflow

    sf32 af = make_safe(3.0f);
    sf32 bf = make_safe(2.0f);
    sf32 cf = af + bf;               // Safe addition
    sf32 df = af / make_safe(0.0f);  // Safe division

    return 0;
}