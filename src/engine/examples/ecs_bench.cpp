#define ENABLE_TESTING
#define ENABLE_THROW // used to verify failure
#include "../ecs.hpp"
#include <chrono>


struct PhysicsComponent {
    struct { float x, y, z; } position;
};

std::ostream& operator<<(std::ostream& os, const PhysicsComponent& pc) {
    os << "PhysicsComponent: { position: (" << pc.position.x << ", " << pc.position.y << ") }";
    return os;
}

struct GraphicsComponent {
    int textureID;
};

std::ostream& operator<<(std::ostream& os, const GraphicsComponent& gc) {
    os << "GraphicsComponent: { textureID: " << gc.textureID << " }";
    return os;
}

struct StatsComponent {
    int health;
};

std::ostream& operator<<(std::ostream& os, const StatsComponent& sc) {
    os << "StatsComponent: { health: " << sc.health << " }";
    return os;
}
void testCreateAndAccessComponents() {
    Lum::ECManager<PhysicsComponent, GraphicsComponent, StatsComponent> ecs;
    
    // Create entities
    auto id1 = ecs.createEntity();
    auto id2 = ecs.createEntity();
    
    // Access components and modify them
    ecs.getEntityComponent<PhysicsComponent>(id1).position.x = 10.0f;
    ecs.getEntityComponent<PhysicsComponent>(id1).position.y = 20.0f;
    ecs.getEntityComponent<GraphicsComponent>(id2).textureID = 5;
    
    // Test values
    assert(ecs.getEntityComponent<PhysicsComponent>(id1).position.x == 10.0f);
    assert(ecs.getEntityComponent<PhysicsComponent>(id1).position.y == 20.0f);
    assert(ecs.getEntityComponent<GraphicsComponent>(id2).textureID == 5);

    std::cout << "Test Create and Access Components: PASSED\n";
}

void testDeleteEntity() {
    Lum::ECManager<PhysicsComponent, GraphicsComponent, StatsComponent> ecs;

    // Create and delete an entity
    auto id1 = ecs.createEntity();
    ecs.getEntityComponent<PhysicsComponent>(id1).position.x = 42.0f;
    ecs.destroyEntity(id1);
    
    // Call update to remove dead entities
    ecs.update();
    
    // Trying to access the deleted entity should throw an exception
    try {
        ecs.getEntityComponent<PhysicsComponent>(id1);
        ASSERT(false); // Should not reach here
    } catch (const std::exception&) {
    std::cout << "Test Delete Entity: PASSED\n";
    }
}

void testShiftComponentsOnUpdate() {
    Lum::ECManager<PhysicsComponent, GraphicsComponent, StatsComponent> ecs;

    // Create entities
    auto id1 = ecs.createEntity();
    auto id2 = ecs.createEntity();
    
    // Assign component values
    ecs.getEntityComponent<PhysicsComponent>(id1).position.x = 10.0f;
    ecs.getEntityComponent<GraphicsComponent>(id1).textureID = 22;
    ecs.getEntityComponent<GraphicsComponent>(id2).textureID = 3;

    // Delete first entity
    ecs.destroyEntity(id1);

    // Update to shift components and remove the deleted entity
    ecs.update();

    // The second entity should still exist and have the same component values
    // std::cout << ecs.getEntityComponent<GraphicsComponent>(id2).textureID << "\n";
    assert(ecs.getEntityComponent<GraphicsComponent>(id2).textureID == 3);

    // Test that the internal index map was updated correctly
    try {
        ecs.getEntityComponent<PhysicsComponent>(id1);
        assert(false); // Should not reach here
    } catch (const std::exception&) {
        std::cout << "Test Shift Components on Update: PASSED\n";
    }
}

void testMultipleDeletesAndUpdates() {
    Lum::ECManager<PhysicsComponent, GraphicsComponent, StatsComponent> ecs;

    // Create multiple entities
    auto id1 = ecs.createEntity();
    auto id2 = ecs.createEntity();
    auto id3 = ecs.createEntity();

    // Assign component values
    ecs.getEntityComponent<PhysicsComponent>(id1).position.x = 10.0f;
    ecs.getEntityComponent<GraphicsComponent>(id2).textureID = 5;
    ecs.getEntityComponent<StatsComponent>(id3).health = 100;

    // Delete some entities
    ecs.destroyEntity(id1);
    ecs.destroyEntity(id2);

    // Update and check the remaining entity
    ecs.update();
    
    // Only id3 should be alive
    assert(ecs.getEntityComponent<StatsComponent>(id3).health == 100);

    // Test that deleted entities no longer exist
    try {
        ecs.getEntityComponent<PhysicsComponent>(id1);
        assert(false); // Should not reach here
    } catch (const std::exception&) {}

    try {
        ecs.getEntityComponent<GraphicsComponent>(id2);
        assert(false); // Should not reach here
    } catch (const std::exception&) {
        std::cout << "Test Multiple Deletes and Updates: PASSED\n";
    }
}

struct ComponentA {
    int data;
};

struct ComponentB {
    float value;
};

template <typename Func>
void benchmark(const std::string& testName, Func&& func, int iterations = 100) {
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        func();  // Call the provided function
    }
    auto end = std::chrono::high_resolution_clock::now();

    // Calculate total elapsed time as a duration
    std::chrono::duration<double> totalElapsed = end - start;
    double totalElapsedSeconds = totalElapsed.count(); // Duration in seconds
    double averageElapsed = totalElapsedSeconds / static_cast<double>(iterations); // Average per iteration

    std::cout << testName << " - Total time: " << totalElapsedSeconds << " seconds\n";
    std::cout << testName << " - Average time: " << averageElapsed << " seconds\n";
}

void benchCreateDestroy() {
    Lum::ECManager<int> ecs;
    std::vector<Lum::EntityID> ids(1000000);
    // Define the lambda for a single entity cached creation iteration
    auto singleCreation = [&]() {
        Lum::EntityID new_id = ecs.createEntity();
        ids.push_back(new_id);
    };
    benchmark("ECS Creation", singleCreation, 1000000);

    // Define the lambda for a single entity deletion iteration
    auto singleDeletion = [&]() {
        Lum::EntityID id = ids.back(); 
        ids.pop_back();
        ecs.destroyEntity(id);
    };
    benchmark("ECS Deletion", singleDeletion, 1000000);
    std::cout<< "\n";
}

void benchVectorCreation() {
    std::vector<int> vec(1000000);

    auto singleCreation = [&]() {
        volatile int a = 42;
        vec.push_back(int(a));
    };
    benchmark("Vector Creation", singleCreation, 1000000);

    auto singleDeletion = [&]() {
        volatile int a = vec.back();
        vec.pop_back();
    };
    benchmark("Vector Deletion", singleDeletion, 1000000);
    std::cout<< "\n";
}

void benchVectorRealistic() {
    constexpr size_t entityCount = 1000000;

    // Separate storage for components
    std::vector<PhysicsComponent> physics(entityCount);
    std::vector<GraphicsComponent> graphics(entityCount);
    std::vector<StatsComponent> stats(entityCount);

    // Initialize components
    for (size_t i = 0; i < physics.size(); ++i) physics[i] = {{1.0f, 2.0f, 3.0f}};
    for (size_t i = 0; i < graphics.size(); ++i) graphics[i] = {static_cast<int>(i % 100)};
    for (size_t i = 0; i < stats.size(); ++i) stats[i] = {100 - static_cast<int>(i % 100)};

    // Benchmark component updates
    auto updateComponents = [&]() {
        for (auto& phys : physics) phys.position.x += (volatile float) 1.0f;
        for (auto& gfx : graphics) gfx.textureID = (gfx.textureID + 1) % (volatile int) 100;
        for (auto& stat : stats) stat.health += (volatile int) 1;
    };

    benchmark("Vector - Realistic Updates", updateComponents, 1000);
}

// Define systems (functions operating on components)
auto physicsSystem  = [](PhysicsComponent& phys) {
    phys.position.x += (volatile float) 1.0f;
};
auto graphicsSystem = [](GraphicsComponent& gfx) {
    gfx.textureID = (gfx.textureID + 1) % (volatile int) 100;
};
auto statsSystem = [](StatsComponent& stats) {
    stats.health += (volatile int) 1;
};

auto combinedSystem = [](PhysicsComponent& phys, GraphicsComponent& gfx, StatsComponent& stats) {
    phys.position.x += 1.0f;
    gfx.textureID = (gfx.textureID + 1) % 100;
    stats.health += 1;
};

// Benchmark ECS using forEachEntityWith
void benchECSRealistic() {
    Lum::ECManager<PhysicsComponent, GraphicsComponent, StatsComponent> ecs;
    constexpr size_t entityCount = 1000000;

    // Create entities with varied components
    for (size_t i = 0; i < entityCount; ++i) {
        auto id = ecs.createEntity();
        if (i % 2 == 0) ecs.getEntityComponent<PhysicsComponent>(id).position = {1.0f, 2.0f, 3.0f};
        if (i % 3 == 0) ecs.getEntityComponent<GraphicsComponent>(id).textureID = i % 100;
        if (i % 5 == 0) ecs.getEntityComponent<StatsComponent>(id).health = 100 - (i % 100);
    }


    // Benchmark systems
    benchmark("ECS - Physics System", [&]() {
        ecs.forEachEntityWith(physicsSystem);
    });

    benchmark("ECS - Graphics System", [&]() {
        ecs.forEachEntityWith(graphicsSystem);
    });

    benchmark("ECS - Stats System", [&]() {
        ecs.forEachEntityWith(statsSystem);
    });

    benchmark("ECS - Combined System", [&]() {
        // ecs.forEachEntityWith(combinedSystem);
        ecs.forEachEntityWith(physicsSystem);
        ecs.forEachEntityWith(graphicsSystem);
        ecs.forEachEntityWith(statsSystem);
    });
}

int main() {
#ifdef ENABLE_TESTING
    testCreateAndAccessComponents();
    testDeleteEntity();
    testShiftComponentsOnUpdate();
    testMultipleDeletesAndUpdates();
#endif

    benchCreateDestroy();
    benchVectorCreation();

    benchVectorRealistic();
    benchECSRealistic();

    return 0;
}
