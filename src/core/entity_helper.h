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
  Entities temp_entities;
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

  static void reserve_temp_space() {
    EntityHelper::get().temp_entities.reserve(sizeof(EntityType) * 100);
  }

  static Entities &get_temp() { return EntityHelper::get().temp_entities; }

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
  static void merge_entity_arrays();
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

// Include full Entity definition after EntityHelper is defined
// This allows entity_helper.h method implementations that need Entity to work
#include "entity.h"

// Implement EntityHelper methods that need Entity to be complete
namespace afterhours {

inline Entity &EntityHelper::createEntity() {
  return createEntityWithOptions({.is_permanent = false});
}

inline Entity &EntityHelper::createPermanentEntity() {
  return createEntityWithOptions({.is_permanent = true});
}

inline Entity &EntityHelper::createEntityWithOptions(const CreationOptions &options) {
  if (get_temp().capacity() == 0) [[unlikely]]
    reserve_temp_space();

  std::shared_ptr<Entity> e(new Entity());
  get_temp().push_back(e);

  if (options.is_permanent) {
    EntityHelper::get().permanant_ids.insert(e->id);
  }

  return *e;
}

inline void EntityHelper::merge_entity_arrays() {
  if (get_temp().empty())
    return;

  for (const auto &entity : get_temp()) {
    if (!entity)
      continue;
    if (entity->cleanup)
      continue;
    get_entities_for_mod().push_back(entity);
  }
  get_temp().clear();
}

template <typename Component>
inline void EntityHelper::registerSingleton(Entity &ent) {
  const ComponentID id = components::get_type_id<Component>();

  if (EntityHelper::get().singletonMap.contains(id)) {
    log_error("Already had registered singleton {}", type_name<Component>());
  }

  EntityHelper::get().singletonMap.emplace(id, &ent);
  log_info("Registered singleton {} for {} ({})", ent.id,
           type_name<Component>(), id);
}

template <typename Component>
inline RefEntity EntityHelper::get_singleton() {
  const ComponentID id = components::get_type_id<Component>();
  auto &singleton_map = EntityHelper::get().singletonMap;
  if (!singleton_map.contains(id)) {
    log_warn("Singleton map is missing value for component {} ({}). Did you "
             "register this component previously?",
             id, type_name<Component>());
    static Entity dummy_entity;
    return dummy_entity;
  }
  auto *entity_ptr = singleton_map.at(id);
  if (!entity_ptr) {
    log_error("Singleton map contains null pointer for component {} ({})",
              id, type_name<Component>());
    static Entity dummy_entity;
    return dummy_entity;
  }
  return *entity_ptr;
}

template <typename Component>
inline Component *EntityHelper::get_singleton_cmp() {
  Entity &ent = get_singleton<Component>();
  return &(ent.get<Component>());
}

inline void EntityHelper::markIDForCleanup(const int e_id) {
  const auto &entities = get_entities();
  auto it = entities.begin();
  while (it != get_entities().end()) {
    if ((*it)->id == e_id) {
      (*it)->cleanup = true;
      break;
    }
    it++;
  }
}

inline void EntityHelper::cleanup() {
  EntityHelper::merge_entity_arrays();
  Entities &entities = get_entities_for_mod();

  const auto newend = std::remove_if(
      entities.begin(), entities.end(),
      [](const auto &entity) { return !entity || entity->cleanup; });

  entities.erase(newend, entities.end());
}

inline void EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL() {
  Entities &entities = get_entities_for_mod();
  entities.clear();
  EntityHelper::get().temp_entities.clear();
}

inline void EntityHelper::delete_all_entities(const bool include_permanent) {
  EntityHelper::merge_entity_arrays();

  if (include_permanent) {
    delete_all_entities_NO_REALLY_I_MEAN_ALL();
    return;
  }

  Entities &entities = get_entities_for_mod();

  const auto newend = std::remove_if(
      entities.begin(), entities.end(), [](const auto &entity) {
        return !EntityHelper::get().permanant_ids.contains(entity->id);
      });

  entities.erase(newend, entities.end());
}

inline void EntityHelper::forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
  for (const auto &e : get_entities()) {
    if (!e)
      continue;
    const auto fef = cb(*e);
    if (fef == 1)
      continue;
    if (fef == 2)
      break;
  }
}

inline std::shared_ptr<Entity> EntityHelper::getEntityAsSharedPtr(const Entity &entity) {
  for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
    if (entity.id == current_entity->id)
      return current_entity;
  }
  return {};
}

inline std::shared_ptr<Entity> EntityHelper::getEntityAsSharedPtr(const OptEntity entity) {
  if (!entity)
    return {};
  const Entity &e = entity.asE();
  return getEntityAsSharedPtr(e);
}

inline OptEntity EntityHelper::getEntityForID(const EntityID id) {
  if (id == -1)
    return {};

  for (const auto &e : get_entities()) {
    if (!e)
      continue;
    if (e->id == id)
      return *e;
  }
  return {};
}

inline Entity &EntityHelper::getEntityForIDEnforce(const EntityID id) {
  auto opt_ent = getEntityForID(id);
  return opt_ent.asE();
}

} // namespace afterhours
