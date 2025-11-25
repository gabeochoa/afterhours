#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "../debug_allocator.h"
#include "../singleton.h"
#include "component_fingerprint_storage.h"
#include "component_storage_registry.h"

// Forward declarations to break circular dependency with entity.h
namespace afterhours {
struct Entity;
struct OptEntity;
using EntityID = int;
using ComponentBitSet = std::bitset<max_num_components>;
using RefEntity = std::reference_wrapper<Entity>;
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

  Entities entities_DO_NOT_USE;
  std::set<int> permanant_ids;
  std::map<ComponentID, Entity *> singletonMap;
  
  // SOA storage
  ComponentFingerprintStorage fingerprint_storage;
  ComponentStorageRegistry component_registry;

  struct CreationOptions {
    bool is_permanent;
  };

  enum ForEachFlow {
    NormalFlow = 0,
    Continue = 1,
    Break = 2,
  };

  static Entities &get_entities_for_mod() {
    return EntityHelper::get().entities_DO_NOT_USE;
  }
  static const Entities &get_entities() { return get_entities_for_mod(); }

  static RefEntities get_ref_entities() {
    RefEntities matching;
    for (const auto &e : EntityHelper::get_entities()) {
      if (!e)
        continue;
      matching.push_back(*e);
    }
    return matching;
  }

  // Forward declarations - implementations after Entity is defined
  static Entity &createEntity();
  static Entity &createPermanentEntity();
  static Entity &createEntityWithOptions(const CreationOptions &options);

  // Forward declarations - implementations after Entity is defined
  template <typename Component> static void registerSingleton(Entity &ent);
  template <typename Component> static RefEntity get_singleton();
  template <typename Component> static Component *get_singleton_cmp();
  static void markIDForCleanup(const int e_id);
  static void cleanup();
  static void delete_all_entities_NO_REALLY_I_MEAN_ALL();
  static void delete_all_entities(const bool include_permanent);

  static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb);
  static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity);
  static std::shared_ptr<Entity> getEntityAsSharedPtr(const OptEntity entity);
  static OptEntity getEntityForID(const EntityID id);
  static Entity &getEntityForIDEnforce(const EntityID id);
  
  // SOA helper methods
  template <typename Component>
  static ComponentStorage<Component> &get_component_storage() {
    return get().component_registry.get_storage<Component>();
  }
  
  static ComponentFingerprintStorage &get_fingerprint_storage() {
    return get().fingerprint_storage;
  }
  
  template <typename Component>
  static Component *get_component_for_entity(EntityID eid) {
    auto &storage = get_component_storage<Component>();
    return storage.get_component(eid);
  }
  
  template <typename Component>
  static const Component *get_component_for_entity_const(EntityID eid) {
    auto &storage = get_component_storage<Component>();
    return storage.get_component(eid);
  }
  
  static ComponentBitSet get_fingerprint_for_entity(EntityID eid) {
    return get_fingerprint_storage().get_fingerprint(eid);
  }
  
  static void update_fingerprint_for_entity(EntityID eid, const ComponentBitSet &fingerprint) {
    get_fingerprint_storage().update_fingerprint(eid, fingerprint);
  }
};

} // namespace afterhours

// Note: EntityHelper method implementations that need Entity are defined
// in entity.h after Entity is complete (see end of entity.h)
