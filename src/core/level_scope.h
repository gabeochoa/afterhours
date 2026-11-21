#pragma once

#include <functional>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "entity.h"
#include "entity_helper.h"

// Level-scoped entities/caches with unload and test-reset hooks.
//
// A level is a LevelId (0 is reserved for unscoped global content). Entities
// created under a ScopedLevel guard are stamped with that level; unload_level()
// deletes exactly those entities and runs the cache-clear callbacks registered
// for the level. Unscoped entities are never touched by unload_level().
namespace afterhours::level_scope {

// RAII guard: entities created while it lives are stamped with `id`. Guards
// nest; leaving one restores the level that was current before it.
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

// The level a creation happening now would be stamped with.
inline LevelId current_level() { return current_level_tls; }

// Single accessors: function-local statics in separate inline functions are
// distinct objects, so every function goes through these.
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

// Register a callback that runs when unload_level(id) runs, e.g. to drop a
// level-keyed cache. Multiple callbacks per level run in registration order;
// unload forgets them, so each unload fires them exactly once.
inline void register_cache_clear(LevelId id, std::function<void()> cb) {
  cache_clear_registry()[id].push_back(std::move(cb));
}

// Run the cache-clear callbacks for id and forget them. Shared by unload and
// reset so a level's caches cannot fire twice.
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

// Delete every entity stamped with id (singletons registered on them go too,
// via cleanup()) and run the level's cache-clear callbacks. Other levels and
// unscoped entities are untouched.
inline void unload_level(LevelId id) {
  if (id == kUnscopedLevel)
    return;
  auto &coll = EntityHelper::get_default_collection();
  // Merge first so a level entity created this frame is marked in the list
  // cleanup() sweeps, singleton registrations included.
  coll.merge_entity_arrays();
  for (auto &sp : coll.get_entities_for_mod()) {
    if (sp && sp->level_id == id)
      sp->cleanup = true;
  }
  coll.cleanup();
  run_and_forget_cache_clears(id);
}

// Entities currently stamped with id, counting pre-merge ones too.
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

// Test-reset hook: fired by reset_all_levels(), for per-test global state that
// owns no level (session counters, shared singletons). Hooks persist across
// resets; clear them to stop firing.
inline void on_test_reset(std::function<void()> cb) {
  test_reset_hooks().push_back(std::move(cb));
}

inline void clear_test_reset_hooks() { test_reset_hooks().clear(); }

// Unload every level that has entities, then fire the test-reset hooks.
// Unscoped entities survive. For tests/level switches that want a clean level
// slate without the nuclear delete_all_entities() (which also drops global
// content).
inline void reset_all_levels() {
  auto &coll = EntityHelper::get_default_collection();
  std::set<LevelId> levels;
  const auto collect = [&levels](const Entities &ents) {
    for (const auto &sp : ents) {
      if (sp && sp->level_id != kUnscopedLevel)
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

// Forget every cache-clear and test-reset registration. For tests that
// re-register per test and must not accumulate other tests' callbacks.
inline void clear_registrations() {
  test_reset_hooks().clear();
  cache_clear_registry().clear();
}

} // namespace afterhours::level_scope
