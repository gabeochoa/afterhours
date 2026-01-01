#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <vector>

#include "../debug_allocator.h"
#include "../singleton.h"
#include "entity.h"

// Include entity_collection.h first so EntityCollection is fully defined
#include "entity_collection.h"

namespace afterhours {
using EntityType = std::shared_ptr<Entity>;

#ifdef AFTER_HOURS_ENTITY_ALLOC_DEBUG
using EntityAllocator = developer::DebugAllocator<EntityType>;
#else
using EntityAllocator = std::allocator<EntityType>;
#endif

using Entities = std::vector<EntityType, EntityAllocator>;
using RefEntities = std::vector<RefEntity>;

SINGLETON_FWD(EntityHelper)
struct EntityHelper {
    SINGLETON(EntityHelper)

    // Default collection for backward compatibility
    EntityCollection default_collection;

    enum ForEachFlow {
        NormalFlow = 0,
        Continue = 1,
        Break = 2,
    };

    // Get the thread-local default collection pointer
    // Returns a reference to the thread-local pointer (nullptr if not set)
    static EntityCollection *&get_thread_default_collection_ptr() {
        // Use function-local static thread_local for C++11 compatibility
        thread_local static EntityCollection *thread_default_collection =
            nullptr;
        return thread_default_collection;
    }

    // Register a collection as the default for the current thread
    // Pass nullptr to unregister and fall back to the singleton's default
    static void set_default_collection(EntityCollection *collection) {
        get_thread_default_collection_ptr() = collection;
    }

    // Get the default collection for the current thread
    // Returns the thread-local collection if registered, otherwise the
    // singleton's default
    static EntityCollection &get_default_collection() {
        EntityCollection *thread_collection =
            get_thread_default_collection_ptr();
        if (thread_collection != nullptr) {
            return *thread_collection;
        }
        return EntityHelper::get().default_collection;
    }

    // Static methods that delegate to default collection (backward
    // compatibility)
    static void reserve_temp_space() {
        get_default_collection().reserve_temp_space();
    }

    static Entities &get_temp() { return get_default_collection().get_temp(); }

    static Entities &get_entities_for_mod() {
        return get_default_collection().get_entities_for_mod();
    }
    static const Entities &get_entities() {
        return get_default_collection().get_entities();
    }

    // Static methods that delegate to default collection (backward
    // compatibility)
    static EntityHandle::Slot bump_gen(EntityHandle::Slot gen) {
        return EntityCollection::bump_gen(gen);
    }

    static EntityHandle::Slot alloc_slot_index() {
        return get_default_collection().alloc_slot_index();
    }

    static void ensure_id_mapping_size(const EntityID id) {
        get_default_collection().ensure_id_mapping_size(id);
    }

    static void assign_slot_to_entity(const EntityType &sp) {
        get_default_collection().assign_slot_to_entity(sp);
    }

    static void invalidate_entity_slot_if_any(const EntityType &sp) {
        get_default_collection().invalidate_entity_slot_if_any(sp);
    }

    static EntityHandle handle_for(const Entity &e) {
        return get_default_collection().handle_for(e);
    }

    static OptEntity resolve(const EntityHandle h) {
        return get_default_collection().resolve(h);
    }

    static RefEntities get_ref_entities() {
        RefEntities matching;
        for (const auto &e : get_entities()) {
            if (!e) continue;
            matching.push_back(*e);
        }
        return matching;
    }

    static Entity &createEntity() {
        return get_default_collection().createEntity();
    }

    static Entity &createPermanentEntity() {
        return get_default_collection().createPermanentEntity();
    }

    static Entity &createEntityWithOptions(
        const EntityCollection::CreationOptions &options) {
        return get_default_collection().createEntityWithOptions(options);
    }

    static void merge_entity_arrays() {
        get_default_collection().merge_entity_arrays();
    }

    template<typename Component>
    static void registerSingleton(Entity &ent) {
        get_default_collection().registerSingleton<Component>(ent);
    }

    template<typename Component>
    static RefEntity get_singleton() {
        return get_default_collection().get_singleton<Component>();
    }

    template<typename Component>
    static Component *get_singleton_cmp() {
        return get_default_collection().get_singleton_cmp<Component>();
    }

    template<typename Component>
    static bool has_singleton() {
        return get_default_collection().has_singleton<Component>();
    }

    static void markIDForCleanup(const int e_id) {
        get_default_collection().markIDForCleanup(e_id);
    }

    static void cleanup() { get_default_collection().cleanup(); }

    static void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
        get_default_collection().delete_all_entities_NO_REALLY_I_MEAN_ALL();
    }

    static void delete_all_entities(const bool include_permanent) {
        get_default_collection().delete_all_entities(include_permanent);
    }

    static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
        for (const auto &e : get_entities()) {
            if (!e) continue;
            const auto fef = cb(*e);
            if (fef == 1) continue;
            if (fef == 2) break;
        }
    }

    static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) {
        for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
            if (entity.id == current_entity->id) return current_entity;
        }
        return {};
    }

    static std::shared_ptr<Entity> getEntityAsSharedPtr(
        const OptEntity entity) {
        if (!entity) return {};
        const Entity &e = entity.asE();
        return getEntityAsSharedPtr(e);
    }

    static OptEntity getEntityForID(const EntityID id) {
        return get_default_collection().getEntityForID(id);
    }

    static Entity &getEntityForIDEnforce(const EntityID id) {
        return get_default_collection().getEntityForIDEnforce(id);
    }
};

}  // namespace afterhours
