#pragma once

#include "entity.h"
#include "entity_handle.h"
#include "entity_helper.h"

namespace afterhours {

// Pointer-free, optionally-resolvable entity reference for persisted state.
//
// - Stores only IDs/handles (no pointers, no reference wrappers).
// - Resolves to a live entity at runtime via EntityHelper.
// - Safe against stale references: when an entity is deleted and its slot is
//   reused, the handle generation ensures old references stop resolving.
//
// Name rationale:
// - We already have `RefEntity` (reference_wrapper) and `OptEntity` (optional
//   reference_wrapper). This type is the "optional entity handle" equivalent.
struct OptEntityHandle {
  EntityID id = -1;
  EntityHandle handle = EntityHandle::invalid();

  static OptEntityHandle from_entity(const Entity &e) {
    OptEntityHandle r;
    r.id = e.id;
    r.handle = EntityHelper::handle_for(e);
    return r;
  }

  [[nodiscard]] OptEntity resolve() const {
    if (handle.valid()) {
      if (auto e = EntityHelper::resolve(handle); e.valid()) {
        return e;
      }
    }
    // Fallback for cases where a handle wasn't available (e.g. temp
    // pre-merge) or was invalidated; ID lookup is O(1) under the handle
    // store.
    if (id >= 0) {
      return EntityHelper::getEntityForID(id);
    }
    return {};
  }
};

} // namespace afterhours
