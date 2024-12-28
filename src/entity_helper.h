#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "base_component.h"
#include "entity.h"
namespace afterhours {

using Entities = std::vector<std::shared_ptr<Entity>>;
using RefEntities = std::vector<RefEntity>;
static std::set<int> permanant_ids;
using EntityMap = std::map<EntityID, std::shared_ptr<Entity>>;

static EntityMap entity_map_DO_NOT_USE;

struct EntityHelper {
  struct CreationOptions {
    bool is_permanent;
  };

  static const EntityMap &get_entities() { return entity_map_DO_NOT_USE; }
  static EntityMap &get_entities_for_mod() { return entity_map_DO_NOT_USE; }
  static std::vector<std::shared_ptr<Entity>> get_ptrs() {
    std::vector<std::shared_ptr<Entity>> ref;
    for (auto &pair : entity_map_DO_NOT_USE) {
      ref.push_back(pair.second);
    }
    return ref;
  }
  static RefEntities get_refs_for_mod() {
    RefEntities ref;
    for (auto &pair : entity_map_DO_NOT_USE) {
      ref.push_back(*pair.second);
    }
    return ref;
  }

  static const RefEntities get_refs() {
    RefEntities ref;
    for (auto &pair : entity_map_DO_NOT_USE) {
      ref.push_back(*pair.second);
    }
    return ref;
  }

  static Entity &createEntity();
  static Entity &createPermanentEntity();
  static Entity &createEntityWithOptions(const CreationOptions &options);

  static void markIDForCleanup(int e_id);
  static void removeEntity(int e_id);
  static void cleanup();
  static void delete_all_entities_NO_REALLY_I_MEAN_ALL();
  static void delete_all_entities(bool include_permanent = false);

  enum ForEachFlow {
    NormalFlow = 0,
    Continue = 1,
    Break = 2,
  };

  static void forEachEntity(const std::function<ForEachFlow(Entity &)> &cb);

  static OptEntity getEntityForID(EntityID id);

  template <typename ContainerT, typename PredicateT>
  static void erase_if(ContainerT &items, const PredicateT &predicate) {
    for (auto it = items.begin(); it != items.end();) {
      if (predicate(*it))
        it = items.erase(it);
      else
        ++it;
    }
  }
};

struct CreationOptions {
  bool is_permanent;
};

Entity &EntityHelper::createEntity() {
  return createEntityWithOptions({.is_permanent = false});
}

Entity &EntityHelper::createPermanentEntity() {
  return createEntityWithOptions({.is_permanent = true});
}

Entity &EntityHelper::createEntityWithOptions(const CreationOptions &options) {
  std::shared_ptr<Entity> e(new Entity());
  entity_map_DO_NOT_USE[e->id] = e;

  if (options.is_permanent) {
    permanant_ids.insert(e->id);
  }

  return *e;
}

void EntityHelper::markIDForCleanup(int e_id) {
  if (entity_map_DO_NOT_USE[e_id])
    entity_map_DO_NOT_USE[e_id]->cleanup = true;
}

void EntityHelper::removeEntity(int e_id) { entity_map_DO_NOT_USE.erase(e_id); }

void EntityHelper::cleanup() {
  // Cleanup entities marked cleanup
  EntityHelper::erase_if(entity_map_DO_NOT_USE, [](const auto &pair) {
    std::shared_ptr<Entity> entity = pair.second;
    return !entity || entity->cleanup;
  });
}

void EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL() {
  // just clear the whole thing
  entity_map_DO_NOT_USE.clear();
}

void EntityHelper::delete_all_entities(bool include_permanent) {
  if (include_permanent) {
    delete_all_entities_NO_REALLY_I_MEAN_ALL();
    return;
  }

  // Only delete non perms
  EntityHelper::erase_if(entity_map_DO_NOT_USE, [](const auto &pair) {
    if (permanant_ids.contains(pair.first))
      return false;
    std::shared_ptr<Entity> entity = pair.second;
    return !entity || entity->cleanup;
  });
}

enum class ForEachFlow {
  NormalFlow = 0,
  Continue = 1,
  Break = 2,
};

OptEntity EntityHelper::getEntityForID(EntityID id) {
  if (id == -1)
    return {};
  if (!entity_map_DO_NOT_USE.contains(id))
    return {};
  auto ptr = entity_map_DO_NOT_USE.at(id);
  if (!ptr)
    return {};
  return *ptr;
}
} // namespace afterhours
