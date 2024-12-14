#define ENABLE_TESTING
#define ENABLE_THROW // used to verify failure
#include "../ecs.hpp"
#include <iostream>

struct ComponentA {
    int valueA;
};

struct ComponentB {
    float valueB;
};

using ComponentC = float;
using ComponentD = int;

// Regular functions
void funA(ComponentB& b, ComponentA& a) {
    std::cout << "    funA: ComponentA value: " << a.valueA << ", ComponentB value: " << b.valueB << ";\n";
}

void funB(ComponentA& a) {
    std::cout << "    funB: ComponentA value: " << a.valueA << ";\n";
}

void funC(ComponentC& c) {
    std::cout << "    funC: ComponentC value: " << c << ";\n";
}

// lambda
auto funD = []() {
    std::cout << "    funD does nothing;\n";
};

int main() {

    using ECManager = Lum::ECManager<ComponentA, ComponentB, ComponentC, ComponentD>;

    ECManager manager;

    // Create an ECSystem with a mix of lambdas and functions
    Lum::ECSystem system(
        manager, // needed for auto type deduction. You can also not use it
        funA,
        funB,
        funC,
        funD // lambda function!
    );

    std::cout << "updating empty ecs\n";
    system.update(manager);
    std::cout << "updated empty ecs\n\n";

    auto e1 = manager.createEntity();
    manager.getEntityComponent<ComponentA&>(e1).valueA = 420;
    auto e2 = manager.createEntity();
    manager.getEntityComponent<ComponentC>(e2) = 888;
    auto e3 = manager.createEntity();
    auto e4 = manager.createEntity();
    manager.getEntityComponent<ComponentA>(e3).valueA = 77;
    manager.getEntityComponent<ComponentC>(e3) = 78;
    manager.getEntityComponent<ComponentD>(e3) = 79;

    std::cout << "updating ecs with 4 ents\n";
    system.update(manager);
    std::cout << "updated ecs with 4 ents\n\n";

    manager.destroyEntity(e1);
    manager.destroyEntity(e3);

    std::cout << "e1/e3 entities destroyed\n";

    std::cout << "updating ecs with 2 ents\n";
    system.update(manager);
    std::cout << "updated ecs with 2 ents\n\n";

    std::cout << "Successfully finished\n\n";

    return 0;
}
