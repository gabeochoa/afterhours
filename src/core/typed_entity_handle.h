#pragma once

#include "entity_handle.h"
#include "entity_helper.h"

namespace afterhours {

// An EntityHandle that records which component the referent is expected to
// carry. The type is documentation first: it turns "some entity" in a
// signature into "the entity that owns a T", and it costs nothing at runtime
// beyond the construction-time check below.
//
// Inherits slot/gen rather than wrapping them, so there is no nested `.handle`
// to reach through and every EntityHandle operation still applies.
template <typename T> struct TypedEntityHandle : EntityHandle {
  TypedEntityHandle() : EntityHandle(EntityHandle::invalid()) {}

  TypedEntityHandle(EntityHandle handle) : EntityHandle(handle) {
    // The invalid sentinel is how a default-initialized member starts life;
    // checking it would log on every such member.
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
  // Logs rather than throws: a missing component is a programming error worth
  // surfacing, but not one worth taking the frame down for.
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
