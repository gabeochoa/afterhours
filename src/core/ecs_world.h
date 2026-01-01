#pragma once

#include <atomic>
#include <cassert>

#include "component_store.h"
#include "entity_helper.h"

namespace afterhours {

// A concrete ECS "world"/registry instance.
// Owns entity lifecycle + component storage so multiple worlds can coexist in
// one process (e.g. host+client).
struct EcsWorld {
  std::atomic_int entity_id_gen{0};
  ComponentStore component_store{};
  EntityHelper entity_helper{};

  EcsWorld();

  [[nodiscard]] EntityID next_entity_id() {
    return static_cast<EntityID>(entity_id_gen.fetch_add(1));
  }
};

// Thread-local "current world" pointer.
// There is intentionally NO process-default fallback: callers must bind a world
// explicitly (including tests/examples).
inline thread_local EcsWorld *g_world = nullptr;

// RAII helper for temporarily switching the current world (per-thread).
struct ScopedWorld {
  EcsWorld *prev = nullptr;
  explicit ScopedWorld(EcsWorld &w) : prev(g_world) { g_world = &w; }
  ~ScopedWorld() { g_world = prev; }
  ScopedWorld(const ScopedWorld &) = delete;
  ScopedWorld &operator=(const ScopedWorld &) = delete;
};

// Legacy/global accessors used by the static ::get() APIs.
[[nodiscard]] inline ComponentStore &global_component_store() {
  assert(g_world && "afterhours: no current world set (use ScopedWorld)");
  return g_world->component_store;
}
[[nodiscard]] inline EntityHelper &global_entity_helper() {
  assert(g_world && "afterhours: no current world set (use ScopedWorld)");
  return g_world->entity_helper;
}

inline EcsWorld::EcsWorld() {
  entity_helper.bind(component_store, entity_id_gen);
}

} // namespace afterhours

