// #define ENABLE_THROW
#include "../ecs.hpp"
#include <iostream>

struct ComponentA {
    int valueA;
};

struct ComponentB {
    float valueB;
};
// typedef float ComponentC;
using ComponentC = float;
using ComponentD = int;
// typedef float ComponentD;

void funA (ComponentB& b, ComponentA& a) {
    std::cout << "    funA: ComponentA value: " << a.valueA << ", ComponentB value: " << b.valueB << ";\n";
}

void funB (ComponentA& a) {
    std::cout << "    funB: ComponentA value: " << a.valueA << ";\n";
}

void funC (ComponentC& c) {
    std::cout << "    funC: ComponentC value: " << c << ";\n";
}

void funD () {
    std::cout << "    funD does nothing" << ";\n";
}

int main() {
    // using factory to simplify creation
    auto ecs = Lum::makeECSystem<
        ComponentA,
        ComponentB,
        ComponentC,
        ComponentD
    >(funA, funB, funC, funD);
    // static_assert(sizeof(ecs)==280);

    std::cout << "updating empty ecs\n";
    ecs.update();
    std::cout << "updated empty ecs\n\n";

    auto e1 = ecs.createEntity();
    ecs.getEntityComponent<ComponentA&>(e1).valueA = 420;
    auto e2 = ecs.createEntity();
    ecs.getEntityComponent<ComponentC&>(e2) = 888;
    auto e3 = ecs.createEntity();
    auto e4 = ecs.createEntity();
    // Not specifying reference is also an option. But it still returns reference
    ecs.getEntityComponent<ComponentA>(e3).valueA = 77;
    ecs.getEntityComponent<ComponentC>(e3) = 78;
    ecs.getEntityComponent<ComponentD>(e3) = 79;

    //another way of accessing
    auto comps3 = ecs.getEntityComponents(e3);
    std::get<ComponentB&>(comps3).valueB = 88;
    

    std::cout << "updating ecs with 4 ents\n";
    ecs.update();
    std::cout << "updated ecs with 4 ents\n\n";

    ecs.destroyEntity(e1);
    ecs.destroyEntity(e3);

    std::cout << "e1/e3 entities destroyed\n";

    std::cout << "updating ecs with 2 ents\n";
    ecs.update();
    std::cout << "updated ecs with 2 ents\n\n";

    return 0;
}