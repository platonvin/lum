#pragma once
#ifndef __ECS_HPP__
#define __ECS_HPP__

#include <cassert>
#include <unordered_map>
#include <tuple>
#include "../macros/assert.hpp"
#include <functional>
#include <type_traits>

#include "../containers/shared_vector.hpp"
#include "parallel/thread_pool.hpp"

template <typename T>
struct debug_type; // make compiler print type

template <typename _Func>
using _func_pointer_t = std::add_pointer_t<_Func>;
// is it possible to do it without macro?
#define func_pointer_t(_Func) _func_pointer_t<decltype(_Func)>

template <typename T>
struct function_traits;

// Specialization for functions and function pointers
template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)> {
    using return_type = Ret;
    using args_type = std::tuple<Args...>;
};

// Specialization for function pointers
template <typename Ret, typename... Args>
struct function_traits<Ret(*)(Args...)> : function_traits<Ret(Args...)> {};

// Specialization for member function pointers
template <typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...)> : function_traits<Ret(Args...)> {};

template <typename Ret, typename Class, typename... Args>
struct function_traits<Ret(Class::*)(Args...) const> : function_traits<Ret(Args...)> {};

// Specialization for std::function
template <typename Ret, typename... Args>
struct function_traits<std::function<Ret(Args...)>> : function_traits<Ret(Args...)> {};

// Specialization for lambda or callable objects
template <typename Callable>
struct function_traits : function_traits<decltype(&Callable::operator())> {};


template <typename Tuple>
struct tuple_with_removed_refs;

template <typename... T>
struct tuple_with_removed_refs<std::tuple<T...>> {
    using type = std::tuple<std::remove_reference_t<T>...>;
};

// alias for easier usage
template <typename Tuple>
using tuple_with_removed_refs_t = typename tuple_with_removed_refs<Tuple>::type;

template <typename Tuple>
struct tuple_tail;

template <typename Head, typename... Tail>
struct tuple_tail<std::tuple<Head, Tail...>> {
    using type = std::tuple<Tail...>;
};

template <typename Tuple>
using tuple_tail_t = typename tuple_tail<Tuple>::type;

namespace Lum {

/**
 * @brief Simple Entity Component System (ECS) data holder template class.
 *
 * @tparam ...Components The component types (structs/classes) managed by the ECS. They HAVE to be different. If you need the same component multiple times, use typedef. This is required for components to be identifiable in compile-time by their type. Typedef just gives them unique "name"
 * @example typedef float hp; typedef float damage; ECManager<hp, damage> ecm{};
 */
using EntityID = long long;  ///< Type alias for entity identifiers.
using InternalIndex = int;  ///< Type alias for entity identifiers.
template<typename... Components>
struct ECManager {
    using AllComponents = std::tuple<Components&...>;
    /** 
     * @brief Nice defualt constructor 
     */
    constexpr ECManager() : nextEntityID(69) {}

    constexpr inline InternalIndex createEntityInternal(){
        // new index is always the last one, because no gaps presented and we want tight packing
        InternalIndex newInternalIndex = componentArrays.size();

        //increase size by one (this is cached. dont worry)
        componentArrays.resize(newInternalIndex+1);

        // mapping from internal to Id
        componentArrays.template get<EntityID>()[newInternalIndex] = nextEntityID;
        componentArrays.template get<bool>()[newInternalIndex] = true;

        ((componentArrays.template get<Components>()[newInternalIndex] = {}), ...);

        // aliveEntities.resize(newInternalIndex + 1, true);
        // (std::get<std::vector<Components>>(componentVectors).resize(newInternalIndex + 1), ...);
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
    constexpr inline void destroyEntity(EntityID id SLOC_DECLARE_DEFARG) {
        auto it = idToInternalIndex.find(id);
        
        ASSERT(it != idToInternalIndex.end(), "Entity with ID " + std::to_string(id) + " not found on Destroy.");

        InternalIndex index = it->second;
        componentArrays.template get<bool>()[index] = false;  // Updated to "aliveEntities"
    }
    template<typename Func>
    constexpr inline void destroyEntity(Func func, EntityID id SLOC_DECLARE_DEFARG) {
        auto it = idToInternalIndex.find(id);

        ASSERT(it != idToInternalIndex.end(), "Entity with ID " + std::to_string(id) + " not found on Destroy.");

        InternalIndex index = it->second;
        loadAndInvoke(func, index);
        componentArrays.template get<bool>()[index] = false;  // Updated to "aliveEntities"
    }
    
    // Reads ONE component from the corresponding vector.
    template<typename Component>
    constexpr inline Component& getEntityComponentInternal(int internal_index) {
        return componentArrays.template get<Component>()[internal_index];
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
    constexpr inline Component& getEntityComponent(EntityID id SLOC_DECLARE_DEFARG) {
        auto it = idToInternalIndex.find(id);
        ASSERT(it != idToInternalIndex.end(), "Entity with ID " + std::to_string(id) + " not found on Get Component.");
        InternalIndex index = it->second;
        ASSERT(index < componentArrays.size(), "Internal index out of bounds.");
        return getEntityComponentInternal<Component>(index);
    }
    
    template<typename ComponentRef>
    requires std::is_reference<ComponentRef>::value
    // ComponentRef is Component& 
    constexpr inline ComponentRef getEntityComponent(EntityID id SLOC_DECLARE_DEFARG) {
        auto it = idToInternalIndex.find(id);
        ASSERT(it != idToInternalIndex.end(), "Entity with ID " + std::to_string(id) + " not found on Get Component.");
        InternalIndex index = it->second;
        ASSERT(index < componentArrays.size(), "Internal index out of bounds.");
        using Component = std::remove_reference<ComponentRef>::type;
        return getEntityComponentInternal<Component>(index);
    }

    /**
     * @brief Retrieves the components of an entity by its ID.
     * 
     * @param id The ID of the entity.
     * @return A tuple containing references to the entity's components.
     * @throws std::runtime_error If the entity does not exist.
     */
    constexpr inline std::tuple<Components&...> getEntityComponents(EntityID id SLOC_DECLARE_DEFARG) {
        auto it = idToInternalIndex.find(id);
        ASSERT(it != idToInternalIndex.end(), "Entity with ID " + std::to_string(id) + " not found on Get Component_S.");
        InternalIndex index = it->second;
        return getEntityComponentsInternal(index);
    }

    constexpr inline std::tuple<Components&...> getEntityComponentsInternal(InternalIndex internal_index) {
        return std::tie(
            // std::get<std::vector<Components>>(componentVectors)[internal_index]...
            componentArrays.template get<Components>()[internal_index]...
            );
    }

    // executes func on every entity, loading needed components and passing them to Func
    // template<typename Func>
    // constexpr inline void forEachEntityWith(Func& func) {
    //     // TODO: assume vectorizable?
    //     #pragma simd
    //     for (size_t i = 0; i < componentArrays.size(); ++i) {
    //         loadAndInvoke<Func>(func, i);
    //     }
    // }

    // executes func on every entity, loading needed components and passing them to Func
    template <typename Func>
    constexpr inline void forEachEntityWith(Func&& func) {
        #pragma simd
        // #pragma GCC ivdep
        for (size_t i = 0; i < componentArrays.size(); ++i) {
            loadAndInvoke(std::forward<Func>(func), i);
        }
    }

    template <typename Func>
    constexpr inline void forEachEntityWith_MT(Func&& func) {
        thread_pool& pool = thread_pool::instance();
        int numThreads = pool.thread_count();

        // Divide the entities into sections
        size_t totalEntities = componentArrays.size();
        size_t sectionSize = (totalEntities + numThreads - 1) / numThreads;

        // Dispatch tasks to the thread pool
        pool.dispatch([this, &func, numThreads, sectionSize, totalEntities](int threadID) {
            size_t start = threadID * sectionSize;
            size_t end = std::min(start + sectionSize, totalEntities);

            for (size_t i = start; i < end; i++) {
                loadAndInvoke(std::forward<Func>(func), (InternalIndex)(i));
            }
        });
    }

    template <typename Func>
    constexpr inline void loadAndInvoke(Func&& func, InternalIndex id) {
        using ArgTuple = typename function_traits<std::decay_t<Func>>::args_type;
        using ArgNoRefTuple = tuple_with_removed_refs_t<ArgTuple>;
        
        // yep could be move up in the pipeline but no actual difference
        if constexpr (std::is_same_v<std::tuple_element_t<0, ArgNoRefTuple>, InternalIndex>) {
            auto components_without_ii = loadComponentsFromTupleInternal<tuple_tail_t<ArgNoRefTuple>>(id);
            auto components = std::tuple_cat(std::make_tuple(id), components_without_ii);
            std::apply(std::forward<Func>(func), components);
        } else {
            auto components = loadComponentsFromTupleInternal<ArgNoRefTuple>(id);
            std::apply(std::forward<Func>(func), components);
        }
    }

    // loads every component function needs and passes it to function
    // template <typename Func>
    // constexpr inline void loadAndInvoke(Func& fun, InternalIndex id) {
    //     using ArgTuple = typename function_traits<Func>::args_type;
    //     using ArgNoRefTuple = tuple_with_removed_refs_t<ArgTuple>;
        
    //     if constexpr (std::is_same_v<std::tuple_element_t<0, ArgNoRefTuple>, InternalIndex>){
    //         // Load components using types provided in function args
    //         auto components_without_ii = loadComponentsFromTupleInternal<tuple_tail_t<ArgNoRefTuple>>(id);
    //         auto components = std::tuple_cat(std::make_tuple(id), components_without_ii);
    //         // Invoke the function with the constructed arguments tuple
    //         std::apply(fun, components);
    //     } else {
    //         // Load components using types provided in function args
    //         auto components = loadComponentsFromTupleInternal<ArgNoRefTuple>(id);
    //         // Invoke the function with the constructed arguments tuple
    //         std::apply(fun, components);
    //     }
    // }


    // Helper function to load components based on a tuple of component types
    // what happens is we iterate over all types-components, load each one of them
    // and put them all (loaded) into a tuple
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
        size_t old_size = componentArrays.size();
        size_t new_size = 0;
        // fun to remove dead enteties components
        // TODO: sort by frequancy? Cache entity which is first to be updated to save iterations?
        auto compactComponents = [&](auto* componentVec) {
            InternalIndex writeIndex = 0;
            for (InternalIndex readIndex = 0; readIndex < componentArrays.size(); readIndex++) {
                if (componentArrays.template get<bool>()[readIndex]) {
                    componentVec[writeIndex] = componentVec[readIndex];
                    writeIndex++;
                }
            }
        };

        // generates code to invoce compactComponents on each pointer in this shared_vector
        // for all "custom" components
        (compactComponents(componentArrays.template get<Components>()), ...);

        // iterate over entities, and remove mappings for dead ones from hashtable
        InternalIndex writeIndex = 0;
        for (InternalIndex readIndex = 0; readIndex < old_size; readIndex++) {
            bool is_read_alive = componentArrays.template get<bool>()[readIndex];
            if (is_read_alive) {
                componentArrays.template get<bool>()[writeIndex] = componentArrays.template get<bool>()[readIndex];
                componentArrays.template get<EntityID>()[writeIndex] = componentArrays.template get<EntityID>()[readIndex];
                writeIndex++;
            } else { // read entity is dead. Remove it from mapping
                // NOTE: currently, this is currently not used, 
                // because mappings are recalculated every frame

                // EntityID deadEntityID = componentArrays.template get<EntityID>()[readIndex];
                // idToInternalIndex.erase(deadEntityID);
            }
        }
        new_size = writeIndex;
        // NOTE: "internal to Id" and "is alive" should not be compacted
        // becuase they already are (above)

        resize(writeIndex);  // Resize the internal structures (component arrays). This is cached

        idToInternalIndex.clear();
        for (InternalIndex i = 0; i < writeIndex; i++) {
            EntityID corresponding_id = componentArrays.template get<EntityID>()[i];
            idToInternalIndex[corresponding_id] = i;
        }
    }

    constexpr inline bool isEntityAlive(EntityID id) {
        auto it = idToInternalIndex.find(id);

        bool presented = (it != idToInternalIndex.end());
        bool alive_internal = false;
        if(presented){
            // std::cout << "a ";
            // alive_internal = getEntityComponentInternal<bool>(it->second);
            alive_internal = getEntityComponent<bool>(id);
            // std::cout << "b ";
        }
        return presented && alive_internal;
    }

    /**
     * @brief Resizes the internal data structures to the specified size.
     *
     * @warning Should not be used more than once per update
     * @param newSize The new size to resize to.
     */
    constexpr inline void resize(int newSize) {
        // (std::get<std::vector<Components>>(componentVectors).resize(newSize), ...);
        // aliveEntities.resize(newSize);
        // internalIndexToID.resize(newSize);
        componentArrays.resize(newSize);
    }
    constexpr inline int size() const {
        return componentArrays.size();
    }

    /**
     * @brief Moves an entity from one internal index to another.
     * 
     * @warning IS PAINFULLY SLOW
     * @param from The index to move the entity from.
     * @param to The index to move the entity to.
     */
    // void moveEntity(InternalIndex from, InternalIndex to) {
    //     if (from != to) {
    //         internalIndexToID[to] = internalIndexToID[from];
    //         (std::swap(std::get<std::vector<Components>>(componentVectors)[from], 
    //                    std::get<std::vector<Components>>(componentVectors)[to]), ...);
    //         componentArrays.template get<bool>()[to] = componentArrays.template get<bool>()[from];
    //     }
    // }

    // this is like vectors, one per component
    shared_vector<
        bool,         ///< Aka cached delete. Flags indicating whether an entity is alive
        EntityID,     ///< Mapping of internal indices to entity IDs.
        Components... ///< Vectors of component data.
    > componentArrays;

    // std::tuple<std::vector<Components>...> componentVectors;  
    // std::vector<bool> aliveEntities; 
    // std::vector<EntityID> internalIndexToID;  
    
    // ankerl::unordered_dense::segmented_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    std::unordered_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    // absl::flat_hash_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    // gtl::btree_map<EntityID, InternalIndex> idToInternalIndex;  ///< Mapping of entity IDs to internal indices.
    EntityID nextEntityID;  ///< ID to be assigned to the next created entity.
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
struct ECSystem {
public:
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

    constexpr inline ECSystem(ECManager_type& ecm, UpdateFuncs... funcs)
        : functions(std::make_tuple(funcs...)) {}
    /**
     * @brief Creates a new entity.
     * 
     * @return EntityID The ID of the newly created entity.
     */
    EntityID createEntity(ECManager_type& ecm){
        return ecm.createEntity();
    }
    /**
     * @brief Destroys an entity.
     * 
     * @param id The ID of the entity to destroy.
     */
    void destroyEntity(ECManager_type& ecm, EntityID id){
        ecm.destroyEntity(id);
    }
    /**
     * @brief Resizes the internal storage for entities.
     * 
     * @param newSize The new size for the entity storage.
     */
    void resize(ECManager_type& ecm, int newSize){
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
    constexpr inline Component& getEntityComponent(ECManager_type& ecm, EntityID id) {
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
    constexpr inline Component& getEntityComponent(ECManager_type& ecm, EntityID id) 
    requires std::is_reference<Component>::value {
        using BaseComponent = std::remove_reference_t<Component>;
        return ecm.template getEntityComponent<BaseComponent>(id);
    }

    /**
     * @brief Retrieves all components of an entity.
     * 
     * @param id The ID of the entity whose components are to be retrieved.
     * @return std::tuple<Components&...> references to all components associated for the specified entity.
     */
    constexpr inline auto getEntityComponents(ECManager_type& ecm, EntityID id) {
        return ecm.getEntityComponents(id);
    }


    /**
     * @brief Updates all systems and the entity manager itself.
     * 
     * Calls the update functions in the order defined in the constructor for all entities.
     */
    constexpr inline void update(ECManager_type& ecm) {
        ecm.update();
        // For each function in the tuple, call ecm.forEachEntityWith
        std::apply([&, this](UpdateFuncs&... funcs) {
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
    constexpr inline void updateSpecific(ECManager_type& ecm, Func fun) {
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
// template <typename... Components, typename... UpdateFuncs>
// constexpr inline auto makeECSystem(UpdateFuncs... funcs) {
//     using ECManager_t = ECManager<Components...>;
//     return ECSystem<ECManager_t, UpdateFuncs...>(funcs...);
// }

} //namespace Lum

#endif // __ECS_HPP__


// struct _ {
//     _() {}
//     _(_&__) {____ = __.____;}
//     _(_&&__) {____ = __.____;}
//     _* ____;

//     _ __(_ ___) {
//         _ _____(___);
//         return _____;
//     };
// };