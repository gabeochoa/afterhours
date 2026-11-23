#pragma once

#include "entity_helper.h"

namespace afterhours {

// Caches a singleton's EntityHandle so repeated fetches skip the singletonMap
// lookup (two unordered_map probes + has/get per call). get() revalidates by
// generation and refetches on mismatch; the component pointer is re-derived
// each call so removal/re-add on the same entity stays safe.
template <typename T> struct CachedSingleton {
  mutable EntityHandle handle{EntityHandle::invalid()};

  [[nodiscard]] T *get() {
    Entity *e = lookup();
    return e ? &e->get<T>() : nullptr;
  }

  [[nodiscard]] const T *get() const {
    const Entity *e = lookup_const();
    return e ? &e->get<T>() : nullptr;
  }

  void reset() { handle = EntityHandle::invalid(); }
  [[nodiscard]] bool valid() const { return handle.is_valid(); }

private:
  Entity *lookup() {
    if (handle.is_valid()) {
      OptEntity e = EntityHelper::resolve(handle);
      if (e && e->has<T>())
        return &e.asE();
      handle = EntityHandle::invalid();
    }
    RefEntity ent = EntityHelper::get_singleton<T>();
    if (!ent.get().has<T>())
      return nullptr;
    const EntityHandle h = EntityHelper::handle_for(ent.get());
    if (h.is_valid())
      handle = h;
    return &ent.get();
  }

  const Entity *lookup_const() const {
    if (handle.is_valid()) {
      OptEntity e = EntityHelper::resolve(handle);
      if (e && e->has<T>())
        return &e.asE();
      handle = EntityHandle::invalid();
    }
    RefEntity ent = EntityHelper::get_singleton<T>();
    if (!ent.get().has<T>())
      return nullptr;
    const EntityHandle h = EntityHelper::handle_for(ent.get());
    if (h.is_valid())
      handle = h;
    return &ent.get();
  }
};

} // namespace afterhours
