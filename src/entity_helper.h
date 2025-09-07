#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "debug_allocator.h"
#include "entity.h"
#include "singleton.h"

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

  Entities entities_DO_NOT_USE;
  Entities temp_entities;
  // TODO spelling
  std::set<int> permanant_ids;
  std::map<ComponentID, Entity *> singletonMap;

  // Component-based entity sets for efficient queries
  std::unordered_map<ComponentID, std::vector<EntityID>> component_entity_sets;

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

  static Entity &createEntity() {
    return createEntityWithOptions({.is_permanent = false});
  }

  static Entity &createPermanentEntity() {
    return createEntityWithOptions({.is_permanent = true});
  }

  static Entity &createEntityWithOptions(const CreationOptions &options) {
    if (get_temp().capacity() == 0) [[unlikely]]
      reserve_temp_space();

    std::shared_ptr<Entity> e(new Entity());
    get_temp().push_back(e);

    if (options.is_permanent) {
      EntityHelper::get().permanant_ids.insert(e->id);
    }

    rebuild_component_sets();

    return *e;
  }

  static void merge_entity_arrays() {
    if (get_temp().empty())
      return;

    for (const auto &entity : get_temp()) {
      if (!entity)
        continue;
      if (entity->cleanup)
        continue;
      get_entities_for_mod().push_back(entity);
    }
    // clear() Leaves the capacity() of a vector unchanged
    get_temp().clear();
  }

  template <typename Component> static void registerSingleton(Entity &ent) {
    const ComponentID id = components::get_type_id<Component>();

    if (EntityHelper::get().singletonMap.contains(id)) {
      log_error("Already had registered singleton {}", type_name<Component>());
    }

    EntityHelper::get().singletonMap.emplace(id, &ent);
    log_info("Registered singleton {} for {} ({})", ent.id,
             type_name<Component>(), id);
  }

  template <typename Component> static RefEntity get_singleton() {
    const ComponentID id = components::get_type_id<Component>();
    if (!EntityHelper::get().singletonMap.contains(id)) {
      log_warn("Singleton map is missing value for component {} ({}). Did you "
               "register this component previously?",
               id, type_name<Component>());
    }
    return *EntityHelper::get().singletonMap[id];
  }

  template <typename Component> static Component *get_singleton_cmp() {
    Entity &ent = get_singleton<Component>();
    return &(ent.get<Component>());
  }

  static void markIDForCleanup(const int e_id) {
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

    const auto newend = std::remove_if(
        entities.begin(), entities.end(),
        [](const auto &entity) { return !entity || entity->cleanup; });

    entities.erase(newend, entities.end());
  }

  static void delete_all_entities_NO_REALLY_I_MEAN_ALL() {
    Entities &entities = get_entities_for_mod();
    // just clear the whole thing
    entities.clear();
    EntityHelper::get().temp_entities.clear();
  }

  static void delete_all_entities(const bool include_permanent) {
    EntityHelper::merge_entity_arrays();

    if (include_permanent) {
      delete_all_entities_NO_REALLY_I_MEAN_ALL();
      return;
    }

    // Only delete non perms
    Entities &entities = get_entities_for_mod();

    const auto newend = std::remove_if(
        entities.begin(), entities.end(), [](const auto &entity) {
          return !EntityHelper::get().permanant_ids.contains(entity->id);
        });

    entities.erase(newend, entities.end());
  }

  static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
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

  // TODO exists as a conversion for things that need shared_ptr right now
  static std::shared_ptr<Entity> getEntityAsSharedPtr(const Entity &entity) {
    for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
      if (entity.id == current_entity->id)
        return current_entity;
    }
    return {};
  }

  static std::shared_ptr<Entity> getEntityAsSharedPtr(const OptEntity entity) {
    if (!entity)
      return {};
    const Entity &e = entity.asE();
    return getEntityAsSharedPtr(e);
  }

  static OptEntity getEntityForID(const EntityID id) {
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

  static Entity &getEntityForIDEnforce(const EntityID id) {
    auto opt_ent = getEntityForID(id);
    return opt_ent.asE();
  }

  // Component set management functions
  template <typename Component>
  static void add_entity_to_component_set(EntityID entity_id) {
    ComponentID comp_id = components::get_type_id<Component>();
    auto &entity_set = get().component_entity_sets[comp_id];

    // Insert in sorted order for efficient intersection
    auto it = std::lower_bound(entity_set.begin(), entity_set.end(), entity_id);
    if (it == entity_set.end() || *it != entity_id) {
      entity_set.insert(it, entity_id);
    }
  }

  template <typename Component>
  static void remove_entity_from_component_set(EntityID entity_id) {
    ComponentID comp_id = components::get_type_id<Component>();
    auto &entity_set = get().component_entity_sets[comp_id];

    auto it = std::lower_bound(entity_set.begin(), entity_set.end(), entity_id);
    if (it != entity_set.end() && *it == entity_id) {
      entity_set.erase(it);
    }
  }

  template <typename Component>
  static const std::vector<EntityID> &get_entities_with_component() {
    ComponentID comp_id = components::get_type_id<Component>();
    return get().component_entity_sets[comp_id];
  }

  // Efficient set intersection for any number of components using variadic
  // templates
  template <typename... Components>
  static std::vector<EntityID> intersect_components() {
    if constexpr (sizeof...(Components) == 0) {
      // No components - return all entities
      std::vector<EntityID> result;
      for (const auto &entity : get_entities()) {
        if (entity) {
          result.push_back(entity->id);
        }
      }
      return result;
    } else if constexpr (sizeof...(Components) == 1) {
      // Single component - return entities with that component
      auto result = get_entities_with_component<Components...>();
      return result;
    } else {
      // Multiple components - use recursive intersection
      auto result = intersect_components_recursive<Components...>();
      return result;
    }
  }

private:
  // Helper function for recursive intersection
  template <typename First, typename Second, typename... Rest>
  static std::vector<EntityID> intersect_components_recursive() {
    const auto &set_a = get_entities_with_component<First>();
    const auto &set_b = get_entities_with_component<Second>();

    std::vector<EntityID> result;
    result.reserve(std::min(set_a.size(), set_b.size()));

    std::set_intersection(set_a.begin(), set_a.end(), set_b.begin(),
                          set_b.end(), std::back_inserter(result));

    if constexpr (sizeof...(Rest) > 0) {
      // Continue with remaining components
      return intersect_with_remaining<Rest...>(::std::move(result));
    } else {
      return result;
    }
  }

  template <typename Next, typename... Rest>
  static std::vector<EntityID>
  intersect_with_remaining(std::vector<EntityID> current) {
    const auto &next_set = get_entities_with_component<Next>();

    std::vector<EntityID> result;
    result.reserve(std::min(current.size(), next_set.size()));

    std::set_intersection(current.begin(), current.end(), next_set.begin(),
                          next_set.end(), std::back_inserter(result));

    if constexpr (sizeof...(Rest) > 0) {
      return intersect_with_remaining<Rest...>(::std::move(result));
    } else {
      return result;
    }
  }

  // Alternative hash-based intersection for better performance with many
  // components
  template <typename... Components>
  static std::vector<EntityID> intersect_components_hash() {
    if constexpr (sizeof...(Components) == 0) {
      // No components - return all entities
      std::vector<EntityID> result;
      for (const auto &entity : get_entities()) {
        if (entity) {
          result.push_back(entity->id);
        }
      }
      return result;
    } else if constexpr (sizeof...(Components) == 1) {
      // Single component - return entities with that component
      return get_entities_with_component<Components...>();
    } else {
      // Use hash-based intersection for multiple components
      return intersect_components_hash_impl<Components...>();
    }
  }

private:
  template <typename First, typename... Rest>
  static std::vector<EntityID> intersect_components_hash_impl() {
    const auto &first_set = get_entities_with_component<First>();

    if constexpr (sizeof...(Rest) == 0) {
      return first_set;
    } else {
      // Create hash set from first component
      std::unordered_set<EntityID> hash_set(first_set.begin(), first_set.end());

      // Intersect with remaining components
      return intersect_with_hash_set<Rest...>(::std::move(hash_set));
    }
  }

  template <typename Next, typename... Rest>
  static std::vector<EntityID>
  intersect_with_hash_set(std::unordered_set<EntityID> hash_set) {
    const auto &next_set = get_entities_with_component<Next>();

    std::vector<EntityID> result;
    result.reserve(std::min(hash_set.size(), next_set.size()));

    // Check which entities from next_set exist in hash_set
    for (const auto &entity_id : next_set) {
      if (hash_set.contains(entity_id)) {
        result.push_back(entity_id);
      }
    }

    if constexpr (sizeof...(Rest) > 0) {
      // Update hash set for next iteration
      hash_set.clear();
      hash_set.insert(result.begin(), result.end());
      return intersect_with_hash_set<Rest...>(::std::move(hash_set));
    } else {
      return result;
    }
  }

public:
  // Rebuild all component sets from current entities
  static void rebuild_component_sets() {
    // Clear all existing sets
    get().component_entity_sets.clear();

    // Rebuild from all entities
    for (const auto &entity : get_entities()) {
      if (!entity)
        continue;

      // Add entity to sets for each component it has
      for (size_t i = 0; i < max_num_components; ++i) {
        if (entity->componentSet[i]) {
          ComponentID comp_id = static_cast<ComponentID>(i);
          get().component_entity_sets[comp_id].push_back(entity->id);
        }
      }
    }

    // Sort all sets for efficient intersection
    for (auto &[comp_id, entity_set] : get().component_entity_sets) {
      std::sort(entity_set.begin(), entity_set.end());
    }
  }

  // Manually add/remove entity from component set (for external use)
  static void add_entity_to_component_set(ComponentID comp_id,
                                          EntityID entity_id) {
    auto &entity_set = get().component_entity_sets[comp_id];
    auto it = std::lower_bound(entity_set.begin(), entity_set.end(), entity_id);
    if (it == entity_set.end() || *it != entity_id) {
      entity_set.insert(it, entity_id);
    }
  }

  static void remove_entity_from_component_set(ComponentID comp_id,
                                               EntityID entity_id) {
    auto &entity_set = get().component_entity_sets[comp_id];
    auto it = std::lower_bound(entity_set.begin(), entity_set.end(), entity_id);
    if (it != entity_set.end() && *it == entity_id) {
      entity_set.erase(it);
    }
  }

  // Wrapper functions for Entity component management
  template <typename Component>
  static void add_entity_to_component_set_wrapper(EntityID entity_id) {
    add_entity_to_component_set<Component>(entity_id);
  }

  template <typename Component>
  static void remove_entity_from_component_set_wrapper(EntityID entity_id) {
    remove_entity_from_component_set<Component>(entity_id);
  }
};

} // namespace afterhours
