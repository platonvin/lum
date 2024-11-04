#include "ecs.hpp"
#include <iostream>
#include <memory>

/*
Experiments on what i like to use for games
*/

// base class for all systems
class System {
public:
    virtual ~System() = default;
    virtual void Start() = 0;
    virtual void Update() = 0;
};

template <typename ECSystem_type, typename... Funcs>
class EntityCollection : public System, public Lum::ECSystem<ECSystem_type, Funcs...> {
public:
    // forward constructor args to ECSystem
    template <typename... Args>
    EntityCollection(Args&&... args) : Lum::ECSystem<ECSystem_type, Funcs...>(args...) {}
    
    void Start() override {
        std::cout << "EntityCollection Start\n";
    }
    void Update() override {
        std::cout << "EntityCollection Update\n";
        this->update();  // call update from ECSystem
    }
};

class EntitySingle : public System {
public:
    static EntitySingle& getInstance() {
        static EntitySingle instance; // guaranteed? to be destroyed and instantiated on first use
        return instance;
    }

    void Start() override {
        std::cout << "SingletonEntity Start\n";
    }

    void Update() override {
        std::cout << "SingletonEntity Update\n";
    }

protected:
    EntitySingle() = default; // constructor is protected to prevent instantiation
    EntitySingle(const EntitySingle&) = delete; // delete copy constructor
    EntitySingle& operator=(const EntitySingle&) = delete; // delete copy assignment operator
};

// Engine class to manage and update systems
class Engine {
public:
    void addSystem(std::shared_ptr<System> system) {
        systems.push_back(system);  // add system to the list
    }

    void start() {
        for (auto& system : systems) {
            system->Start();
        }
    }

    void update() {
        for (auto& system : systems) {
            system->Update();
        }
    }

private:   
    // there is rtti involved, but its not that costly for high-level systems and you would do almost the same in code anyways 
    // no rtti inside collections (i hope so at least)
    std::vector<std::shared_ptr<System>> systems;  // vector of system pointers
};