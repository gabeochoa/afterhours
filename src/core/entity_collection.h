#pragma once

#include <atomic>
#include <cassert>

#include "component_store.h"
#include "entity_helper.h"

namespace afterhours {

// A concrete ECS state container.
// Owns entity lifecycle + component storage so multiple independent collections
// can coexist in one process (e.g. host+client, server+client, tests).
struct EntityCollection {
  std::atomic_int entity_id_gen{0};
  ComponentStore cmp_store{};
  EntityHelper entity_helper{};

  EntityCollection();

  [[nodiscard]] EntityID next_entity_id() {
    return static_cast<EntityID>(entity_id_gen.fetch_add(1));
  }
};

// Thread-local "current collection" pointer.
// There is intentionally NO process-default fallback: callers must bind a
// collection explicitly (including tests/examples).
inline thread_local EntityCollection *g_collection = nullptr;

// RAII helper for temporarily switching the current collection (per-thread).
struct ScopedEntityCollection {
  EntityCollection *prev = nullptr;
  explicit ScopedEntityCollection(EntityCollection &w)
      : prev(g_collection) {
    g_collection = &w;
  }
  ~ScopedEntityCollection() { g_collection = prev; }
  ScopedEntityCollection(const ScopedEntityCollection &) = delete;
  ScopedEntityCollection &operator=(const ScopedEntityCollection &) = delete;
};

// Global accessors used by the static ::get() APIs.
[[nodiscard]] inline ComponentStore &global_component_store() {
  assert(g_collection &&
         "afterhours: no current EntityCollection set (use ScopedEntityCollection)");
  return g_collection->cmp_store;
}
[[nodiscard]] inline EntityHelper &global_entity_helper() {
  assert(g_collection &&
         "afterhours: no current EntityCollection set (use ScopedEntityCollection)");
  return g_collection->entity_helper;
}

inline EntityCollection::EntityCollection() {
  entity_helper.bind(cmp_store, entity_id_gen);
}

} // namespace afterhours

