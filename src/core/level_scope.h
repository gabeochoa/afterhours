#pragma once
#include <functional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>
#include "entity.h"
#include "entity_helper.h"
namespace afterhours::level_scope {
struct ScopedLevel {
  LevelId previous;
  explicit ScopedLevel(LevelId id) : previous(current_level_tls) {
    current_level_tls = id;
  }

  ~ScopedLevel() { current_level_tls = previous; }
  ScopedLevel(const ScopedLevel &) = delete;
  ScopedLevel &operator=(const ScopedLevel &) = delete;
};
inline ScopedLevel scoped(LevelId id) { return ScopedLevel{id}; }
inline LevelId current_level() { return current_level_tls; }
inline std::unordered_map<LevelId, std::vector<std::function<void()>>> &
cache_clear_registry() {
  static std::unordered_map<LevelId, std::vector<std::function<void()>>>
      registry;
  return registry;
}

inline std::vector<std::function<void()>> &test_reset_hooks() {
  static std::vector<std::function<void()>> hooks;
  return hooks;
}

inline void register_cache_clear(LevelId id, std::function<void()> cb) {
  cache_clear_registry()[id].push_back(std::move(cb));
}

inline void run_and_forget_cache_clears(LevelId id) {
  auto &registry = cache_clear_registry();
  auto it = registry.find(id);
  if (it == registry.end())
    return;
  auto callbacks = std::move(it->second);
  registry.erase(it);
  for (auto &cb : callbacks)
    cb();
}

inline void unload_level(LevelId id) {
  if (id == UNSCOPED_LEVEL)
    return;
  auto &coll = EntityHelper::get_default_collection();
  coll.merge_entity_arrays();
  for (auto &sp : coll.get_entities_for_mod()) {
    if (sp && sp->level_id == id)
      sp->cleanup = true;
  }
  coll.cleanup();
  run_and_forget_cache_clears(id);
}

inline std::size_t count_in_level(LevelId id) {
  auto &coll = EntityHelper::get_default_collection();
  std::size_t n = 0;
  const auto count = [id, &n](const Entities &ents) {
    for (const auto &sp : ents) {
      if (sp && sp->level_id == id)
        ++n;
    }
  };
  count(coll.get_entities());
  count(coll.get_temp());
  return n;
}

inline void on_test_reset(std::function<void()> cb) {
  test_reset_hooks().push_back(std::move(cb));
}

inline void clear_test_reset_hooks() { test_reset_hooks().clear(); }
inline void reset_all_levels() {
  auto &coll = EntityHelper::get_default_collection();
  std::set<LevelId> levels;
  const auto collect = [&levels](const Entities &ents) {
    for (const auto &sp : ents) {
      if (sp && sp->level_id != UNSCOPED_LEVEL)
        levels.insert(sp->level_id);
    }
  };
  collect(coll.get_entities());
  collect(coll.get_temp());
  for (LevelId id : levels)
    unload_level(id);
  for (auto &cb : test_reset_hooks())
    cb();
}

inline void clear_registrations() {
  test_reset_hooks().clear();
  cache_clear_registry().clear();
}
}
