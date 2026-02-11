#pragma once

#include "../../ecs.h"

namespace afterhours {

namespace ui {

// Owns the separate EntityCollection for UI entities.
// Uses Meyers singleton so the collection lives in the UI plugin.
//
// By default, UI entities live in their own collection, separate from the
// default EntityHelper collection. This improves iteration performance since
// game systems skip UI entities and vice versa.
//
// Define AFTER_HOURS_UI_SINGLE_COLLECTION before including this header to
// put UI entities back into the default collection (pre-split behavior).
// This is useful for migration or if your code queries the default collection
// for UIComponent entities directly.
struct UICollectionHolder {
  EntityCollection own_collection;
  EntityCollection &collection;

  static UICollectionHolder &get() {
    static UICollectionHolder instance;
    return instance;
  }

  // Look up entity by ID: searches UI collection first, then default.
  static OptEntity getEntityForID(EntityID id) {
    auto result = get().collection.getEntityForID(id);
    if (result.valid()) return result;
    return EntityHelper::getEntityForID(id);
  }

  // Look up entity by ID (enforce): searches UI collection first, then default.
  static Entity &getEntityForIDEnforce(EntityID id) {
    auto result = get().collection.getEntityForID(id);
    if (result.valid()) return result.asE();
    return EntityHelper::getEntityForIDEnforce(id);
  }

  UICollectionHolder(const UICollectionHolder &) = delete;
  UICollectionHolder &operator=(const UICollectionHolder &) = delete;

private:
#ifdef AFTER_HOURS_UI_SINGLE_COLLECTION
  UICollectionHolder()
      : collection(EntityHelper::get_default_collection()) {}
#else
  UICollectionHolder() : collection(own_collection) {}
#endif
};

} // namespace ui

} // namespace afterhours
