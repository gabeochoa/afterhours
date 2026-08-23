#pragma once

#include "entity_handle.h"
#include "entity_helper.h"

namespace afterhours {

// EntityHandle that documents (and optionally checks) an expected component.
// Inherits slot/gen -- there is no nested `.handle` member.
template <typename T> struct TypedEntityHandle : EntityHandle {
  // TODO allow implicit conversion from EntityHandle
  TypedEntityHandle() : EntityHandle(EntityHandle::invalid()) {}

  TypedEntityHandle(EntityHandle handle) : EntityHandle(handle) {
    // Skip checks for the empty sentinel used by default member init.
    if (handle.is_valid()) {
      validate_entity(handle);
    }
  }

  TypedEntityHandle(Entity &entity)
      : EntityHandle(EntityHelper::handle_for(entity)) {
    validate_entity(EntityHelper::handle_for(entity));
  }

  static TypedEntityHandle invalid() { return TypedEntityHandle{}; }

  OptEntity operator->() const { return EntityHelper::resolve(*this); }

private:
  static void validate_entity(EntityHandle handle) {
    OptEntity entity = EntityHelper::resolve(handle);
    if (!entity) {
      log_error("TypedEntityHandle::validate_entity() - entity not found");
      return;
    }
    if (!entity->has<T>()) {
      log_error("TypedEntityHandle::validate_entity() - entity is missing T");
    }
  }
};

template <typename T> OptEntity resolve(TypedEntityHandle<T> handle) {
  return EntityHelper::resolve(static_cast<EntityHandle>(handle));
}

} // namespace afterhours
