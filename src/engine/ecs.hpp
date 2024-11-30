#pragma once
#include <string>
#ifndef __ECS_HPP__
#define __ECS_HPP__

#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <tuple>
#include <stdexcept>
#include <iostream>
// #include <chrono>
#include <type_traits>

template <typename T>
struct debug_type; // make compiler print type

template <typename _Func>
using _func_pointer_t = std::add_pointer_t<_Func>;
// is it possible to do it without macro?
#define func_pointer_t(_Func) _func_pointer_t<decltype(_Func)>

template<typename F, typename = void>
struct function_traits;

// specialization for function types
template<typename R, typename ... A>
struct function_traits<R(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

// specialization for function pointer types
template<typename R, typename ... A>
struct function_traits<R(*)(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

// specialization for pointers to member functions
template<typename R, typename G, typename ... A>
struct function_traits<R(G::*)(A...)> {
    using return_type = R;
    using args_type = std::tuple<A...>;
};

template <typename Tuple>
struct tuple_with_removed_refs;

template <typename... T>
struct tuple_with_removed_refs<std::tuple<T...>> {
    using type = std::tuple<std::remove_reference_t<T>...>;
};

// alias for easier usage
template <typename Tuple>
using tuple_with_removed_refs_t = typename tuple_with_removed_refs<Tuple>::type;

namespace Lum {
// #define ENABLE_THROW
// #define DISABLE_CHECKS

#ifdef DISABLE_CHECKS
    #define ASSERT(...) do {} while (false)
#else
    #ifdef ENABLE_THROW
    #define ASSERT(condition)                                    \
        do {                                                     \
            if (!(condition)) {                                  \
                throw std::runtime_error("Assertion failed: "    \
                    #condition ", in " __FILE__ ", line " + std::to_string(__LINE__)); \
            }                                                    \
        } while (false)
    #else
    #define ASSERT(condition)                                    \
        do {                                                     \
            if (!(condition)) {                                  \
                std::cerr << "Assertion failed: " #condition     \
                        << ", in " << __FILE__ << ", line " << __LINE__ << std::endl; \
                std::abort();                                    \
            }                                                    \
        } while (false)
    #endif
#endif



/**
 * @brief Simple Entity Component System (ECS) data holder template class.
 *
 * @tparam ...Components The component types (structs/classes) managed by the ECS. They HAVE to be different. If you need the same component multiple times, use typedef. This is required for components to be identifiable in compile-time by their type. Typedef just gives them unique "name"
 * @example typedef float hp; typedef float damage; ECManager<hp, damage> ecm{};
 */
using EntityID = long long;  ///< Type alias for entity identifiers.
using InternalIndex = int;  ///< Type alias for entity identifiers.
template<typename... Components>
class ECManager {
public:
    using AllComponents = std::tuple<Components&...>;
    /** 
     * @brief Nice defualt constructor 
     */
    constexpr ECManager() : nextEntityID(69), cachedResize(0) {}

    constexpr inline InternalIndex createEntityInternal(){
        InternalIndex newInternalIndex = aliveEntities.size();
        ASSERT(newInternalIndex == internalIndexToID.size());

        internalIndexToID.push_back(nextEntityID);

        aliveEntities.resize(newInternalIndex + 1, true);
        (std::get<std::vector<Components>>(componentVectors).resize(newInternalIndex + 1), ...);
        return newInternalIndex;
    }
    /**
     * @brief Creates a new entity and returns its ID.
     * @details resizes component vectors immediately (turned out to be relatevly ok).
     * @return EntityID The ID of the newly created entity.
     */
    constexpr inline EntityID createEntity() {
        InternalIndex newInternalIndex = createEntityInternal();
        
        EntityID newID = nextEntityID++;
        idToInternalIndex[newID] = newInternalIndex;
        return newID;
    }

    /**
     * @brief Creates a new entity, calls a function func on it and returns its ID.
     * @details resizes component vectors immediately (turned out to be relatevly ok).
     * @return EntityID The ID of the newly created entity.
     */
    template<typename Func>
    constexpr inline EntityID createEntity(Func func) {
        InternalIndex newInternalIndex = createEntityInternal();
        loadAndInvoke(func, newInternalIndex);
        
        EntityID newID = nextEntityID++;
        idToInternalIndex[newID] = newInternalIndex;
        return newID;
    }

    /**
     * @brief Destroys an entity by its ID.
     * 
     * @param id The ID of the entity to be destroyed.
     * @throws std::runtime_error If the entity does not exist.
     */
    void destroyEntity(EntityID id) {
        auto it = idToInternalIndex.find(id);
        ASSERT (it != idToInternalIndex.end());
        InternalIndex index = it->second;
        aliveEntities[index] = false;  // Updated to "aliveEntities"
    }
    template<typename Func>
    void destroyEntity(Func func, EntityID id) {
        auto it = idToInternalIndex.find(id);
        ASSERT (it != idToInternalIndex.end());
        InternalIndex index = it->second;
        loadAndInvoke(func, index);
        aliveEntities[index] = false;  // Updated to "aliveEntities"
    }
    // just reads component from corresponding vector
    template<typename Component>
    Component& getEntityComponentInternal(int index) {
        return std::get<std::vector<Component>>(componentVectors)[index];
    }

    /**
     * @brief Retrieves a specific component of an entity by its ID.
     * 
     * @tparam Component The type of the component to retrieve.
     * @param id The ID of the entity.
     * @return A reference to the requested component.
     * @throws std::runtime_error If the entity does not exist.
     *
     * @todo better syntax
     */
    template<typename Component>
    Component& getEntityComponent(EntityID id) {
        auto it = idToInternalIndex.find(id);
        ASSERT(it != idToInternalIndex.end());
        InternalIndex index = it->second;
        ASSERT(index < aliveEntities.size());
        return getEntityComponentInternal<Component>(index);
    }

    /**
     * @brief Retrieves the components of an entity by its ID.
     * 
     * @param id The ID of the entity.
     * @return A tuple containing references to the entity's components.
     * @throws std::runtime_error If the entity does not exist.
     */
    std::tuple<Components&...> getEntityComponents(EntityID id) {
        auto it = idToInternalIndex.find(id);
        ASSERT (it != idToInternalIndex.end());
        InternalIndex index = it->second;
        return getEntityComponentsInternal(index);
    }

    std::tuple<Components&...> getEntityComponentsInternal(InternalIndex internal_index) {
        return std::tie(std::get<std::vector<Components>>(componentVectors)[internal_index]...);
    }

    template<typename Func>
    // Executes Func on every entity, loading needed components and passing them to Func
    void forEachEntityWith(Func func) {
        // TODO: assume vectorizable?
        // #pragma GCC ivdep
        for (size_t i = 0; i < aliveEntities.size(); ++i) {
            loadAndInvoke<Func>(func, i);
        }
    }
    // loads every component function needs and passes it to function
    template <typename Func>
    constexpr inline void loadAndInvoke(Func fun, InternalIndex id) {
        using ArgTuple = typename function_traits<Func>::args_type;
        using ArgNoRefTuple = tuple_with_removed_refs_t<ArgTuple>;

        auto components = loadComponentsFromTupleInternal<ArgNoRefTuple>(id);

        std::apply(fun, components);
    }


    // Helper function to load components based on a tuple of component types
    template <typename Tuple, std::size_t... I>
    constexpr inline auto loadComponentsFromTupleImplInternal(InternalIndex id, std::index_sequence<I...>) {
        return std::forward_as_tuple(getEntityComponentInternal<std::tuple_element_t<I, Tuple>>(id)...);
    }

    template <typename Tuple>
    constexpr inline auto loadComponentsFromTupleInternal(InternalIndex id) {
        return loadComponentsFromTupleImplInternal<Tuple>(id, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
    }

    // Variadic template to load multiple components
    template <typename... _Components>
    constexpr inline std::tuple<_Components&...> loadComponents(EntityID id) {
        return std::forward_as_tuple(loadComponent<_Components>(id)...);
    }

    /**
     * @brief Updates the ECS, removing dead entities and compacting component vectors
     */
    void update() {
        // remove dead enteties components 
        auto compactComponents = [&](auto& componentVec) {
            InternalIndex writeIndex = 0;
            for (InternalIndex readIndex = 0; readIndex < aliveEntities.size(); readIndex++) {
                if (aliveEntities[readIndex]) {
                    componentVec[writeIndex] = componentVec[readIndex];
                    writeIndex++;
                }
            }
        };

        // for all components
        (compactComponents(std::get<std::vector<Components>>(componentVectors)), ...);

        // same buf for "if alive" component
        InternalIndex writeIndex = 0;
        for (InternalIndex readIndex = 0; readIndex < aliveEntities.size(); readIndex++) {
            if (aliveEntities[readIndex]) {
                aliveEntities[writeIndex] = aliveEntities[readIndex];
                internalIndexToID[writeIndex] = internalIndexToID[readIndex];
                writeIndex++;
            } else {
                EntityID deadEntityID = internalIndexToID[readIndex];
                idToInternalIndex.erase(deadEntityID);
            }
        }

        resize(writeIndex);  // Resize the internal structures
        idToInternalIndex.clear();
        for (InternalIndex i = 0; i < writeIndex; i++) {
            idToInternalIndex[internalIndexToID[i]] = i;
        }

        // for next cached usage
        cachedResize = aliveEntities.size();
        ASSERT(cachedResize == internalIndexToID.size());
    }

private:
    /**
     * @brief Resizes the internal data structures to the specified size.
     *
     * @warning Should not be used more than once per update
     * @param newSize The new size to resize to.
     */
    void resize(int newSize) {
        (std::get<std::vector<Components>>(componentVectors).resize(newSize), ...);
        aliveEntities.resize(newSize);
        internalIndexToID.resize(newSize);
    }

    /**
     * @brief Moves an entity from one internal index to another.
     * 
     * @warning IS PAINFULLY SLOW
     * @param from The index to move the entity from.
     * @param to The index to move the entity to.
     */
    void moveEntity(InternalIndex from, InternalIndex to) {
        if (from != to) {
            internalIndexToID[to] = internalIndexToID[from];
            (std::swap(std::get<std::vector<Components>>(componentVectors)[from], 
                       std::get<std::vector<Components>>(componentVectors)[to]), ...);
            aliveEntities[to] = aliveEntities[from];
        }
    }

    std::tuple<std::vector<Components>...> componentVectors;  ///< Vectors of component data.
    /* Components that are always presented */
    std::vector<bool> aliveEntities;  ///< Aka cached delete. Flags indicating whether an entity is alive
    std::vector<EntityID> internalIndexToID;  ///< Mapping of internal indices to entity IDs.
    //TODO: find best umap. Ankerl?
    
    // ankerl::unordered_dense::segmented_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    std::unordered_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    // absl::flat_hash_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    // gtl::btree_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    EntityID nextEntityID;  ///< ID to be assigned to the next created entity.
    InternalIndex cachedResize; ///< Size component vectors will be qual to. Cached prevent resizing on every createEntity()
};

// Utility to expand the tuple into individual types
template <typename Func, typename Tuple>
struct ComponentFunctionHelper;

template <typename Func, typename... Components>
struct ComponentFunctionHelper<Func, std::tuple<Components...>> {
    // Check that Func is callable with references of Components and that Components are references
    static constexpr inline bool value = requires(Func func, Components&... components) {
        { func(components...) } -> std::same_as<void>;
    } && (std::is_lvalue_reference_v<Components> && ...);  // Ensure all are lvalue references
};

// Concept to check if a function can take a specific set of component references as arguments
template <typename Func, typename Tuple>
concept FunctionSatisfiesArguments = ComponentFunctionHelper<Func, Tuple>::value;

// c++ syntax is crazy, but it does the job
/**
 * @brief Simple Entity Component System (ECS) template class.
 * 
 * This class defines a template for an Entity Component System, allowing for the management 
 * of entities and their associated components, as well as the systems that operate on them.
 * The class supports a variadic number of component types and a variadic number of update functions.
 * 
 * @tparam ECManager_type The Entity Components (data) Manager class with components defined.
 * @tparam OnInitFunc Function called for every entity when it is created.
 * @tparam OnDestroyFunc Function called for every entity when it is destroyed.
 * @tparam UpdateFuncs Systems (functions) that operate on this ECS. They are called in a given order every frame (update).
 * 
 * @details The class defines ECS data as well as systems (functions) that operate on it.
 * 
 * @todo Implement synchronization so systems can be reordered.
 * @todo Parallelize update calls for improved performance.
 */
template <typename ECManager_type, typename... UpdateFuncs> 
class ECSystem {
public:
    ECManager_type ecm;
    std::tuple<UpdateFuncs...> functions;

    /**
     * @brief Constructs the ECSystem with initialization, cleanup and variadic update functions.
     * 
     * @param init Function called for each entity when created.
     * @param cleanup Function called for each entity when destroyed.
     * @param funcs Variadic list of update functions to operate on entities.
     */
    constexpr inline ECSystem(UpdateFuncs... funcs)
        : functions(std::make_tuple(funcs...)) {}

    constexpr inline ECSystem(ECManager_type& ecsInstance, UpdateFuncs... funcs)
        : ecm(ecsInstance), functions(std::make_tuple(funcs...)) {}

    /**
     * @brief Creates a new entity.
     * 
     * @return EntityID The ID of the newly created entity.
     */
    EntityID createEntity(){
        return ecm.createEntity();
    }
    /**
     * @brief Destroys an entity.
     * 
     * @param id The ID of the entity to destroy.
     */
    void destroyEntity(EntityID id){
        ecm.destroyEntity(id);
    }
    /**
     * @brief Resizes the internal storage for entities.
     * 
     * @param newSize The new size for the entity storage.
     */
    void resize(int newSize){
        ecm.resize(newSize);
    }
    /**
     * @brief Retrieves a specific component from a given entity.
     * 
     * @tparam Component The type& of the component to retrieve.
     * @param id The ID of the entity from which to retrieve the component.
     * @return Component& reference to the requested component.
     */
    template<typename Component>
    constexpr inline Component& getEntityComponent(EntityID id) {
        return ecm.template getEntityComponent<Component>(id);
    }
    /**
     * @brief Retrieves a specific component from a given entity.
     * 
     * @tparam Component The type (no reference - its just syntax sugar overload) of the component to retrieve.
     * @param id The ID of the entity from which to retrieve the component.
     * @return Component& reference to the requested component.
     */
    template<typename Component>
    constexpr inline Component& getEntityComponent(EntityID id) requires std::is_reference<Component>::value {
        using BaseComponent = std::remove_reference_t<Component>;
        return ecm.template getEntityComponent<BaseComponent>(id);
    }

    /**
     * @brief Retrieves all components of an entity.
     * 
     * @param id The ID of the entity whose components are to be retrieved.
     * @return std::tuple<Components&...> references to all components associated for the specified entity.
     */
    constexpr inline auto getEntityComponents(EntityID id) {
        return ecm.getEntityComponents(id);
    }


    /**
     * @brief Updates all systems and the entity manager itself.
     * 
     * Calls the update functions in the order defined in the constructor for all entities.
     */
    constexpr inline void update() {
        ecm.update();
        // For each function in the tuple, call ecm.forEachEntityWith
        std::apply([this](UpdateFuncs&... funcs) {
            (ecm.forEachEntityWith(funcs), ...);
        }, functions);
    }

    /**
     * @brief Invokes a specific system(function) for all entities.
     * 
     * @tparam Func The type of the function to invoke.
     * @param fun The function to be called for each entity.
     * @details just call ecs.updateSpecific(YourFunction);
     */
    template <typename Func>
    constexpr inline void updateSpecific(Func fun) {
        ecm.forEachEntityWith(fun);
    }
};

/**
 * @brief Factory function to create an ECSystem instance.
 * 
 * This function simplifies the creation of an ECSystem by automatically generating the 
 * Entity Component Manager and function types.
 * 
 * @tparam Components The component types that will be managed by the ECManager.
 * @tparam OnInitFunc The type of the function called for every entity when it is created.
 * @tparam OnDestroyFunc The type of the function called for every entity when it is destroyed.
 * @tparam UpdateFuncs Variadic types representing the systems (functions) that operate on this ECS.
 * 
 * @param init Function to initialize an entity upon creation.
 * @param cleanup Function to clean up an entity upon destruction.
 * @param funcs Variadic list of update functions to operate on entities.
 * 
 * @return An instance of the ECSystem
 */
template <typename... Components, typename... UpdateFuncs>
constexpr inline auto makeECSystem(UpdateFuncs... funcs) {
    using ECManager_t = ECManager<Components...>;
    return ECSystem<ECManager_t, UpdateFuncs...>(funcs...);
}

} //namespace Lum

#endif // __ECS_HPP__