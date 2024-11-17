#include <cstddef>
#include <tuple>
#include <type_traits>

// literally Offset Calculator, mostly for PromisedOpaqueStorage 

template <typename... Types>
class OffsetCalculator {
private:
    std::tuple<Types...> typeTuple;

    template <std::size_t Index>
    constexpr std::size_t calculateOffset() {
        if constexpr (Index == 0) {
            return 0;
        } else {
            return calculateOffset<Index - 1>() + sizeof(std::tuple_element_t<Index - 1, std::tuple<Types...>>);
        }
    }

public:
    // Get offset of type at index
    template <std::size_t Index>
    constexpr std::size_t offset()  {
        return calculateOffset<Index>();
    }

    // Get the offset of specified type
    template <typename T>
    constexpr std::size_t get() {
        constexpr std::size_t index = findIndex<T>();
        static_assert(index < sizeof...(Types), "Type not found in OffsetCalculator");
        return OffsetCalculator::template offset<index>();
    }

    // Find index of type within the param pack
    template <typename T, std::size_t Index = 0>
    constexpr std::size_t findIndex() {
        if constexpr (Index < sizeof...(Types)) {
            using TypeAtIdx = std::tuple_element_t<Index, std::tuple<Types...>>;
            if constexpr (std::is_same_v<T, TypeAtIdx>) {
                return Index;
            } else  {
                return findIndex<T, Index + 1>();
            }
        } else {
            return sizeof...(Types);  // Not found, returns the size of the pack
        }
    }

    constexpr std::size_t size() {
        return (sizeof(Types) + ...);
    }
};


// small wrapper to make accessing PromisedOpaqueStorage easier
// constexpr instead of inline ?
namespace TypedStorageHelper {
    template <typename T, typename Storage, typename... Types>
    inline T& get(Storage& storage) {
        constexpr std::size_t offset = OffsetCalculator<Types...>::template get<T>();
        return storage.template get<T, offset>();
    }

    template <typename T, typename Storage, typename... Types>
    inline const T& get(const Storage& storage) {
        constexpr std::size_t offset = OffsetCalculator<Types...>::template get<T>();
        return storage.template get<T, offset>();
    }
}