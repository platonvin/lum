#pragma once
#ifndef __LUM_SHARED_VECTOR_HPP__
#define __LUM_SHARED_VECTOR_HPP__

// wrapper for "vectors" of components, each different type and all the same size
// compared to std::vector's, has less overhead for such group usage
// because there is only one allocation and only one size var. Used by ECS
// not indendent to be used without a wrapper (like ecs)

#include <tuple>
#include <cstring>
#include <tuple>
#include <cstring>
#include <type_traits>

template <typename... Types>
class shared_vector {
private:
    // Tuple of pointers to memory blocks for each type
    std::tuple<Types*...> data_pointers = {};

    // Number of elements in the container
    size_t size_ = 0;

    // Total allocated size (used for capacity management)
    size_t capacity_ = 0;

    // Raw allocated memory for all elements
    uint8_t* allocated_mem_ptr = nullptr;

    // Calculate the offset of each type in the contiguous memory block
    template <std::size_t Index>
    constexpr size_t get_type_offset() const {
        if constexpr (Index == 0) {
            return 0;
        } else {
            return get_type_offset<Index - 1>() +
                   sizeof(std::tuple_element_t<Index - 1, std::tuple<Types...>>);
        }
    }

    // Allocate memory for a given capacity
    uint8_t* allocate_memory(size_t capacity) const {
        return new uint8_t[capacity * (sizeof(Types) + ...)];
    }

    // Copy data from old memory to new memory during resizing
    template <std::size_t Index>
    constexpr void copy_data_recursive(uint8_t* new_memory, size_t old_size, size_t new_capacity) const {
        using T = std::tuple_element_t<Index, std::tuple<Types...>>;
        T* old_ptr = std::get<Index>(data_pointers);
        T* new_ptr = reinterpret_cast<T*>(new_memory + get_type_offset<Index>() * new_capacity);

        for (size_t i = 0; i < old_size; ++i) {
            new (new_ptr + i) T(old_ptr[i]); // Placement new to copy construct
            old_ptr[i].~T();                // Explicitly call destructor for old objects
        }

        if constexpr (Index > 0) {
            copy_data_recursive<Index - 1>(new_memory, old_size, new_capacity);
        }
    }

    // Destroy all objects in memory
    template <std::size_t Index>
    constexpr void destroy_objects_recursive() {
        using T = std::tuple_element_t<Index, std::tuple<Types...>>;
        T* ptr = std::get<Index>(data_pointers);

        for (size_t i = 0; i < size_; ++i) {
            ptr[i].~T();
        }

        if constexpr (Index > 0) {
            destroy_objects_recursive<Index - 1>();
        }
    }

public:
    // Constructor
    shared_vector() = default;

    // Destructor
    ~shared_vector() {
        if (allocated_mem_ptr) {
            // destroy_objects_recursive<sizeof...(Types) - 1>();
            delete[] allocated_mem_ptr;
        }
    }

    // Resize the container
    // Cached here cause im 80 iq, sorry for that
    constexpr void resize(size_t new_size) {
        if (new_size > capacity_) {
            size_t new_capacity = std::max(new_size, capacity_ * 2);
            uint8_t* new_memory = allocate_memory(new_capacity);

            if (allocated_mem_ptr) {
                copy_data_recursive<sizeof...(Types) - 1>(new_memory, size_, new_capacity);
                // destroy_objects_recursive<sizeof...(Types) - 1>();
                delete[] allocated_mem_ptr;
            }

            allocated_mem_ptr = new_memory;
            capacity_ = new_capacity;

            size_t offset = 0;
            ((std::get<Types*>(data_pointers) =
              reinterpret_cast<Types*>(allocated_mem_ptr + offset),
              offset += sizeof(Types) * new_capacity),
             ...);
        }

        size_ = new_size;
    }

    // Access specific vector by index
    template <std::size_t Index>
    constexpr auto* get() {
        return std::get<Index>(data_pointers);
    }
    template <std::size_t Index>
    constexpr const auto* get() const {
        return std::get<Index>(data_pointers);
    }

    // Access specific vector by type
    template <typename TypeToAcces>
    constexpr auto* get() {
        return std::get<TypeToAcces*>(data_pointers);
    }
    template <typename TypeToAcces>
    requires std::is_reference<TypeToAcces>::value
    constexpr auto* get() {
        // return std::get<TypeToAcces*>(data_pointers);
        return std::get<std::remove_reference<TypeToAcces>*>(data_pointers);
    }
    // template <typename TypeToAcces>
    // const auto* get() {
    //     return std::get<TypeToAcces*>(data_pointers);
    // }

    // Get the current size
    constexpr inline size_t size() const {
        return size_;
    }

    // Get the current capacity
    constexpr inline size_t capacity() const {
        return capacity_;
    }
};


#endif //__LUM_SHARED_VECTOR_HPP__