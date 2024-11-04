# Lum::ECSystem 
minimalistic C++ template class implementing an Entity Component System (ECS). Provides an interface for runtime managing (create/destroy) entities and comptime* onStart / onUpdate / onDestroy functions for entities

*there is no runtime code that would decide which components should be loaded. In other words, compiler knows in compile-time which components to load for each function (to then pass them as arguments to functions). This also means that all the component and systems have to be known in compile-time (via template and constructor args)

## Features 
 
- **Compact Memory** : Update() the ECS to compact memory by removing unused entities. After that, all components are in plain arrays (vectors)

- **Minimal Runtime Overhead** : No extra runtime code compared to writing everything manually. It also means that library mostly syntax sugar. But every library can be viewed as just syntax sugar. 

## Usage 
 
1. **Define Components** : Components are types (e.g., structs or classes) associated with entities.

```cpp
struct ComponentA { int valueA; };
struct ComponentB { float valueB; };
```
 
2. **Define systems (functions)** : Systems are function that take component&'s. There have to be 2 special system - init and destroy. Order of component& arguments does not matter

```cpp
void init (ComponentA& a, ComponentB& b) {
    a = {0xA};
    b = {0xB};
    std::cout << "  init called\n";
}
void cleanup (ComponentB& b, ComponentA& a) {
    std::cout << "  cleanup called\n";
}

void funA (ComponentA& a, ComponentB& b) {
    std::cout << "    funA: ComponentA value: " << a.valueA << ", ComponentB value: " << b.valueB << ";\n";
}
```

3. **Initialize ECSystem** : use factory to make ECSystem. Functions init and cleanup have to be presented

```cpp
auto ecs = Lum::makeECSystem<
        ComponentA,
        ComponentB
    >(init, cleanup, funA /*any amount of other systems-that-are-functions*/);
```
 
4. **Create Entities** : Instantiate entities, each automatically receiving a unique ID.

```cpp
auto entityID = ecs.createEntity();
```

5. **Retrieve and modify individual entity components** :

```cpp
ecs.getEntityComponent<ComponentA&>(entityID).valueA = 100;
// accessing as not-reference is also an option. No actual difference - just style
ecs.getEntityComponent<ComponentB>(entityID).valueB = 100.0f;
```
 
6. **Update the ECS** : Call `update()` to remove "dead" entities and invoke every system. Makes destroyed entities handles invalid

```cpp
ecs.update();
```

7. **Destroy Entities** : Mark entity destroy, making its handle invalid to access *after* next Update() called

```cpp
ecs.destroyEntity(entityID);
```

## Configuration 

- `#define` `ENABLE_THROW` before including ecs.hpp to use throw'ing std::runtime_error. If not defined, assert()'s are used.

- `#define` `DISABLE_CHECKS` before including ecs.hpp to remove runtime validation