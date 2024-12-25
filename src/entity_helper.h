#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "entity.h"
namespace afterhours {

using Entities = std::vector<RefEntity>;
using RefEntities = std::vector<RefEntity>;

static Entities entities_DO_NOT_USE;
static std::set<int> permanant_ids;

struct EntityHelper {
  struct CreationOptions {
    bool is_permanent;
  };

  static const Entities &get_entities();
  static Entities &get_entities_for_mod();
  static RefEntities get_ref_entities();

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
};

Entities &EntityHelper::get_entities_for_mod() { return entities_DO_NOT_USE; }
const Entities &EntityHelper::get_entities() { return get_entities_for_mod(); }

RefEntities EntityHelper::get_ref_entities() {
  RefEntities matching;
  for (Entity &e : EntityHelper::get_entities()) {
    matching.push_back(e);
  }
  return matching;
}

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
  auto e = new Entity();
  get_entities_for_mod().push_back(*e);

  if (options.is_permanent) {
    permanant_ids.insert(e->id);
  }

  return *e;
}

void EntityHelper::markIDForCleanup(int e_id) {
  auto &entities = get_entities();
  auto it = entities.begin();
  while (it != get_entities().end()) {
    Entity &entity = *it;
    if (entity.id == e_id) {
      entity.cleanup = true;
      break;
    }
    it++;
  }
}

void EntityHelper::removeEntity(int e_id) {
  auto &entities = get_entities_for_mod();

  auto newend = std::remove_if(
      entities.begin(), entities.end(),
      [e_id](const Entity &entity) { return entity.id == e_id; });

  entities.erase(newend, entities.end());
}

void EntityHelper::cleanup() {
  // Cleanup entities marked cleanup
  Entities &entities = get_entities_for_mod();

  auto newend =
      std::remove_if(entities.begin(), entities.end(),
                     [](const Entity &entity) { return entity.cleanup; });

  for (auto it = newend; it != entities.end(); it++) {
    Entity &e = *it;
    delete &e;
  }

  entities.erase(newend, entities.end());
}

void EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL() {
  Entities &entities = get_entities_for_mod();
  // just clear the whole thing
  entities.clear();
}

void EntityHelper::delete_all_entities(bool include_permanent) {
  if (include_permanent) {
    delete_all_entities_NO_REALLY_I_MEAN_ALL();
    return;
  }

  // Only delete non perms
  Entities &entities = get_entities_for_mod();

  auto newend = std::remove_if(
      entities.begin(), entities.end(),
      [](const Entity &entity) { return !permanant_ids.contains(entity.id); });

  entities.erase(newend, entities.end());
}

enum class ForEachFlow {
  NormalFlow = 0,
  Continue = 1,
  Break = 2,
};

void EntityHelper::forEachEntity(
    const std::function<ForEachFlow(Entity &)> &cb) {
  for (Entity &e : get_entities()) {
    auto fef = cb(e);
    if (fef == 1)
      continue;
    if (fef == 2)
      break;
  }
}

OptEntity EntityHelper::getEntityForID(EntityID id) {
  if (id == -1)
    return {};

  for (Entity &e : get_entities()) {
    if (e.id == id)
      return e;
  }
  return {};
}
} // namespace afterhours
