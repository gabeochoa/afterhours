#pragma once

#include <atomic>

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

// Process-default world for legacy APIs.
// Prefer passing/owning an EcsWorld explicitly when you need isolation.
[[nodiscard]] inline EcsWorld &default_world() {
  static EcsWorld w{};
  return w;
}

// Thread-local "current world" pointer.
// This is the minimal way to make the legacy `::get()` APIs world-aware without
// forcing a world parameter through every call site.
inline thread_local EcsWorld *g_world = nullptr;

[[nodiscard]] inline EcsWorld &current_world() {
  return g_world ? *g_world : default_world();
}

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
  return current_world().component_store;
}
[[nodiscard]] inline EntityHelper &global_entity_helper() {
  return current_world().entity_helper;
}

inline EcsWorld::EcsWorld() {
  entity_helper.bind(component_store, entity_id_gen);
}

} // namespace afterhours

