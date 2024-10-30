#pragma once
#include <cassert>
#ifndef __ARENA_HPP__
#define __ARENA_HPP__

// extracted from [Circulli Bellum] (https://github.com/platonvin/circuli-Bellum)
// simple arena allocator with faster allocation time

#include <vector>
// #include <macros.hpp>
#include "ankerl_unordered_dense.hpp"

// ~2x faster than new&delete + possibly more coherent memory
// this really was the bottlneck

//TODO: test uset against garbage vector

template<typename Type>
class Arena {
public:
    std::vector<Type> storage;
    ankerl::unordered_dense::set<int> free_indices;
    // std::vector<int> free_indices; //TODO: test against me please please please

    Arena(int initial_size) : _size(initial_size){
        storage.resize(initial_size);
        free_indices.reserve(initial_size);
        for (int i = 0; i < initial_size; i++) {
            free_indices.insert(i);
        }
    }

    ~Arena() {
        storage.clear();
        free_indices.clear();
    }

    void clear() {
        // freeIndices.clear();
        for (int i = 0; i < storage.size(); i++) {
            free_indices.insert(i);
        }
    }

    // void resize(int new_size) {
    //     int old_size = storage.size();
    //     pl(old_size)
    //     pl(new_size)
    //     storage.resize(new_size);
    //     // free_indices.reserve(new_size);
    //     for (int i = old_size; i < new_size; i++) {
    //         free_indices.insert(i);
    //     }
    // }

    Type* allocate() {
        if (free_indices.empty()) [[unlikely]] {
            assert(false && "arena full\n");
            exit(1);
        }

        int index = *free_indices.begin();
        free_indices.erase(index);
        //called a constructor, just in case
        storage[index] = Type();

        // return new Type;
        return &storage[index];
    }

    void free(Type* obj_ptr) {
        // delete obj_ptr;
        (*obj_ptr).~Type();
        int idx = (obj_ptr - &storage[0]);
        assert(!free_indices.contains(idx));
        free_indices.insert(idx);

        // std::cout << "freed " << (long long)obj_ptr << "\n";
    }
    int total_size() {return _size;}
private:
    int _size;
};
#endif // __ARENA_HPP__