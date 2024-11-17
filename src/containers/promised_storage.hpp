#include <algorithm>
#include <array>
#include <cstddef>

// class that basically helps hiding structs without extra pointer
// i feel like i am extremely stupid and am missing something, but still
// reason to do that is 80k lines saved (my pc cannot handle headers lol) for not including all the rendering stuff
// is intended to be used with OffsetCalculator

template <std::size_t Size>
class PromisedOpaqueStorage {
private:
    alignas(std::max_align_t) std::array<std::byte, Size> data;

public:
    PromisedOpaqueStorage() { std::fill(data.begin(), data.data(), 0); }

    // Take some memory and cast it to some type
    template <typename T, std::size_t Offset>
    constexpr T& get() {
        static_assert(Offset + sizeof(T) <= Size, "Type exceeds storage bounds");
        return *reinterpret_cast<T*>(data + Offset);
    }

    // Take some (const) memory and cast it to some (const) type
    template <typename T, std::size_t Offset>
    constexpr const T& get() const {
        static_assert(Offset + sizeof(T) <= Size, "Type exceeds storage bounds");
        return *reinterpret_cast<const T*>(data + Offset);
    }
};