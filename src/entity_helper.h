#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "debug_allocator.h"
#include "entity.h"

namespace afterhours {
using EntityType = std::shared_ptr<Entity>;

#ifdef AFTER_HOURS_ENTITY_ALLOC_DEBUG
using EntityAllocator = developer::DebugAllocator<EntityType>;
#else
using EntityAllocator = std::allocator<EntityType>;
#endif

using Entities = std::vector<EntityType, EntityAllocator>;

using RefEntities = std::vector<RefEntity>;

static Entities entities_DO_NOT_USE;
static Entities temp_entities;

// TODO spelling
static std::set<int> permanant_ids;

static std::map<ComponentID, Entity *> singletonMap;

struct EntityHelper {
  struct CreationOptions {
    bool is_permanent;
  };

  enum ForEachFlow {
    NormalFlow = 0,
    Continue = 1,
    Break = 2,
  };

  static void reserve_temp_space() {
    // by default lets only allow 100 ents to be created per frame
    temp_entities.reserve(sizeof(EntityType) * 100);
  }

  static Entities &get_entities_for_mod() { return entities_DO_NOT_USE; }
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

  static Entity &createEntity() {
    return createEntityWithOptions({.is_permanent = false});
  }

  static Entity &createPermanentEntity() {
    return createEntityWithOptions({.is_permanent = true});
  }

  static Entity &createEntityWithOptions(const CreationOptions &options) {
    if (temp_entities.capacity() == 0) [[unlikely]]
      reserve_temp_space();

    std::shared_ptr<Entity> e(new Entity());
    temp_entities.push_back(e);

    if (options.is_permanent) {
      permanant_ids.insert(e->id);
    }

    return *e;
  }

  static void merge_entity_arrays() {
    if (temp_entities.empty())
      return;

    for (const auto &entity : temp_entities) {
      if (!entity)
        continue;
      if (entity->cleanup)
        continue;
      entities_DO_NOT_USE.push_back(entity);
    }
    // clear() Leaves the capacity() of a vector unchanged
    temp_entities.clear();
  }

  template <typename Component> static void registerSingleton(Entity &ent) {
    ComponentID id = components::get_type_id<Component>();

    if (singletonMap.contains(id)) {
      log_error("Already had registered singleton {}", type_name<Component>());
    }

    singletonMap.emplace(id, &ent);
    log_info("Registered singleton {} for {} ({})", ent.id,
             type_name<Component>(), id);
  }

  template <typename Component> static RefEntity get_singleton() {
    ComponentID id = components::get_type_id<Component>();
    if (!singletonMap.contains(id)) {
      log_warn("Singleton map is missing value for component {} ({}). Did you "
               "register this component previously?",
               id, type_name<Component>());
    }
    return *singletonMap[id];
  }

  template <typename Component> static Component *get_singleton_cmp() {
    Entity &ent = get_singleton<Component>();
    return &(ent.get<Component>());
  }

  static void markIDForCleanup(int e_id) {
    auto &entities = get_entities();
    auto it = entities.begin();
    while (it != get_entities().end()) {
      if ((*it)->id == e_id) {
        (*it)->cleanup = true;
        break;
      }
      it++;
    }
  }

  // note: I feel like we dont need to give this ability
  // you should be using cleanup=true
  //
  // static void removeEntity(int e_id) {
  // auto &entities = get_entities_for_mod();
  //
  // auto newend = std::remove_if(
  // entities.begin(), entities.end(),
  // [e_id](const auto &entity) { return !entity || entity->id == e_id; });
  // entities.erase(newend, entities.end());
  // }

  static void cleanup() {
    EntityHelper::merge_entity_arrays();
    // Cleanup entities marked cleanup
    //
    Entities &entities = get_entities_for_mod();

    auto newend = std::remove_if(
        entities.begin(), entities.end(),
        [](const auto &entity) { return !entity || entity->cleanup; });

    entities.erase(newend, entities.end());
  }

  static void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
    Entities &entities = get_entities_for_mod();
    // just clear the whole thing
    temp_entities.clear();
  }

  static void delete_all_entities(bool include_permanent) {
    EntityHelper::merge_entity_arrays();

    if (include_permanent) {
      delete_all_entities_NO_REALLY_I_MEAN_ALL();
      return;
    }

    // Only delete non perms
    Entities &entities = get_entities_for_mod();

    auto newend = std::remove_if(
        entities.begin(), entities.end(),
        [](const auto &entity) { return !permanant_ids.contains(entity->id); });

    entities.erase(newend, entities.end());
  }

  static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
    for (const auto &e : get_entities()) {
      if (!e)
        continue;
      auto fef = cb(*e);
      if (fef == 1)
        continue;
      if (fef == 2)
        break;
    }
  }

  // TODO exists as a conversion for things that need shared_ptr right now
  static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) {
    for (std::shared_ptr<Entity> current_entity : get_entities()) {
      if (entity.id == current_entity->id)
        return current_entity;
    }
    return {};
  }

  static std::shared_ptr<Entity> getEntityAsSharedPtr(OptEntity entity) {
    if (!entity)
      return {};
    const Entity &e = entity.asE();
    return getEntityAsSharedPtr(e);
  }

  static OptEntity getEntityForID(EntityID id) {
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

  static Entity &getEntityForIDEnforce(EntityID id) {
    auto opt_ent = getEntityForID(id);
    return opt_ent.asE();
  }
};

} // namespace afterhours
