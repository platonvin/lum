#include <cstddef>
template <typename... Types>
struct TotalSize;

// Base case for recursion
template <>
struct TotalSize<> {
    static constexpr size_t value = 0;
};

// Recursive case: add the size of the first type and recurse on the rest
template <typename First, typename... Rest>
struct TotalSize<First, Rest...> {
    static constexpr size_t value = sizeof(First) + TotalSize<Rest...>::value;
};

// Helper variable template to make the syntax easier
template <typename... Types>
constexpr size_t TotalSize_v = TotalSize<Types...>::value;

// Function to print the size (for easy testing)
template <typename... Types>
constexpr size_t calculateTotalSize() {
    return TotalSize_v<Types...>;
}