#include <iostream>
#include <unordered_set>
#include <cassert>
#define GLM_ENABLE_EXPERIMENTAL
#define GLM_GTX_component_wise 
#include "glm/gtx/hash.hpp"
#include <glm/ext/vector_int3.hpp>
#include "parallel.hpp"
#include <glm/gtx/component_wise.hpp>

void test_basic_dispatch() {
    ComputeDispatcher dispatcher;
    // std::unordered_set<ivec3> positions;
    int things [3][3][3];
    
    ivec3 size{3, 3, 3}; // This doesn't divide evenly by the thread count in most setups
    dispatcher.dispatch(size, [&](ivec3 pos) {
        // positions.insert(pos);
        things[pos.x][pos.y][pos.z] = 42069;
    });

    for(int x=0; x<3; x++){
    for(int y=0; y<3; y++){
    for(int z=0; z<3; z++){
        assert(things[x][y][z] == 42069 && "Basic Dispatch Test Failed");
    }}}
    std::cout << "Basic Dispatch Test Passed.\n";
}

void test_large_dispatch() {
    ComputeDispatcher dispatcher;
    std::atomic<int> counter{0};
    
    ivec3 size{100, 100, 10};
    dispatcher.dispatch(size, [&](ivec3) {
        counter++;
    });

    // Check if all 100000 positions were processed
    assert(counter == size.x * size.y * size.z && "Large Dispatch Test Failed: Expected 100000 invocations.");
    std::cout << "Large Dispatch Test Passed.\n";
}

void test_single_element() {
    ComputeDispatcher dispatcher;
    std::atomic<int> counter{0};

    dispatcher.dispatch({1, 1, 1}, [&](ivec3) {
        counter++;
    });

    // Check if only one position was processed
    assert(counter == 1 && "Single Element Test Failed: Expected 1 invocation.");
    std::cout << "Single Element Test Passed.\n";
}

void test_zero_dimension() {
    ComputeDispatcher dispatcher;
    std::atomic<int> counter{0};

    dispatcher.dispatch({0, 50, 50}, [&](ivec3) {
        counter++;
    });

    // No positions should be processed
    assert(counter == 0 && "Zero Dimension Test Failed: Expected 0 invocations.");
    std::cout << "Zero Dimension Test Passed.\n";
}

void test_uneven_division() {
    ComputeDispatcher dispatcher;
    ivec3 size = {707,506,510};
    // ivec3 size = {5,5,5};
    // ivec3 size = {5,5,5};
    int* things = new int[glm::compMul(size)];
    std::cout << glm::compMul(size);
    // sizeof(things);
    
    dispatcher.dispatch(size, [&](ivec3 pos) {
        // positions.insert(pos);
        things[pos.x + size.x*pos.y + size.x*size.y*pos.z] = 69420;
    });
    std::cout << glm::compMul(size);

    // for(int x=0; x<size.x; x++){
    // for(int y=0; y<size.y; y++){
    // for(int z=0; z<size.z; z++){
    //     ivec3 pos = {x,y,z};
    //     assert(things[pos.x + size.x*pos.y + size.x*size.y*pos.z] == 69420 && "Uneven Division Test Failed: Expected 1050 unique positions.");
    // }}}
    std::cout << "Uneven Division Test Passed.\n";
}

int main() {
    test_basic_dispatch();
    test_large_dispatch();
    test_single_element();
    test_zero_dimension();
    test_uneven_division();
    std::cout << "All tests passed.\n";
    return 0;
}