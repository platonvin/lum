#include "../engine.hpp"


struct ComponentA {
    int valueA;
};

struct ComponentB {
    float valueB;
};
typedef float ComponentC;

void init (ComponentA& a, ComponentB& b, ComponentC& c) {
    a = {0xA};
    b = {0xB};
    c = 0xC;
    std::cout << "  init:" << ";\n";
}
void cleanup (ComponentA& a, ComponentB& b, ComponentC& c) {
    std::cout << "  cleanup: ComponentA value: " << a.valueA << ", ComponentB value: " << b.valueB << ", ComponentC value: " << c << ";\n";
}

void funA (ComponentA& a, ComponentB& b) {
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
    Engine engine;
    
    // auto ecs = Lum::makeECSystem<ComponentA, ComponentB, ComponentC>(
        // init, cleanup, funA, funB, funC, funD);
    auto secs = EntityCollection<Lum::ECManager<
            ComponentA,
            ComponentB,
            ComponentC
        >, decltype(&init), decltype(&cleanup), decltype(&funA), decltype(&funB), decltype(&funC), decltype(&funD)
        >(init, cleanup, funA, funB, funC, funD);

    auto secs_collection = std::make_shared<decltype(secs)>(secs);

        // secs_collection->createEntity();
        // secs_collection->createEntity();

    // Adding systems to the engine
    engine.addSystem(secs_collection);
    engine.addSystem(std::shared_ptr<System>(&EntitySingle::getInstance()));  // Singleton system

    // Starting and updating all systems
    engine.start();
    engine.update();
    engine.update();
    engine.update();

    return 0;
}