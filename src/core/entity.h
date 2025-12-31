#pragma once

#include <atomic>
#include <bitset>
#include <cstdint>
#include <optional>
#include <type_traits>

#include "../logging.h"
#include "../type_name.h"
#include "../bitset_utils.h"
#include "base_component.h"
#include "component_store.h"
#include "entity_id.h"
#include "entity_handle.h"
#include "pointer_policy.h"

namespace afterhours {
#ifndef AFTER_HOURS_MAX_ENTITY_TAGS
#define AFTER_HOURS_MAX_ENTITY_TAGS 64
#endif

using TagId = std::uint8_t;
using TagBitset = std::bitset<AFTER_HOURS_MAX_ENTITY_TAGS>;
template <typename Base, typename Derived> bool child_of(Derived *derived) {
  using BasePtr =
      std::conditional_t<std::is_const_v<Derived>, const Base *, Base *>;
  return dynamic_cast<BasePtr>(derived) != nullptr;
}

using ComponentBitSet = std::bitset<max_num_components>;

static std::atomic_int ENTITY_ID_GEN = 0;

struct Entity {
  EntityID id;
  int entity_type = 0;

  // Runtime-only metadata used by the handle-based store.
  // - INVALID_SLOT means this entity has not been assigned a stable slot yet
  //   (e.g., it's still in temp_entities pre-merge).
  EntityHandle::Slot ah_slot_index = EntityHandle::INVALID_SLOT;

  ComponentBitSet componentSet;

  TagBitset tags{};
  bool cleanup = false;

  Entity() : id(ENTITY_ID_GEN++) {}
  Entity(const Entity &) = delete;
  Entity(Entity &&other) noexcept = default;

  virtual ~Entity() {}

  // Iterate all enabled component type IDs on this entity.
  // NOTE: Current implementation scans the bitset width.
  template <typename Fn> void for_each_component_id(Fn &&fn) const {
    bitset_utils::for_each_enabled_bit(componentSet, [&](const std::size_t i) {
      return fn(static_cast<ComponentID>(i));
    });
  }

  template <typename T> [[nodiscard]] bool has() const {
    const bool result = componentSet[components::get_type_id<T>()];
#if defined(AFTER_HOURS_DEBUG)
    log_trace("checking component {} {} on entity {}",
              components::get_type_id<T>(), type_name<T>(), id);
    log_trace("your set is now {}", componentSet);
    log_trace("and the result was {}", result);
#endif
    return result;
  }

  template <typename T> [[nodiscard]] bool has_child_of() const {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("checking for child components {} {} on entity {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    bool found = false;
    for_each_component_id([&](const ComponentID cid) {
      const BaseComponent *cmp =
          ComponentStore::get().try_get_base(cid, this->id);
      if (!cmp)
        return bitset_utils::ForEachFlow::Continue;
      if (child_of<T>(cmp)) {
        found = true;
        return bitset_utils::ForEachFlow::Break;
      }
      return bitset_utils::ForEachFlow::Continue;
    });
    return found;
  }

  template <typename A, typename B, typename... Rest> bool has() const {
    return has<A>() && has<B, Rest...>();
  }

  template <typename T> [[nodiscard]] bool is_missing() const {
    return !has<T>();
  }

  template <typename A> bool is_missing_any() const { return is_missing<A>(); }

  template <typename A, typename B, typename... Rest>
  bool is_missing_any() const {
    return is_missing<A>() || is_missing_any<B, Rest...>();
  }

  template <typename T> void removeComponent() {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("removing component_id:{} {} to entity_id: {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    if (!this->has<T>()) {
#if defined(AFTER_HOURS_DEBUG)
      log_error("trying to remove but this entity {} doesnt have the "
                "component attached {} {}",
                id, components::get_type_id<T>(), type_name<T>());
#endif
      return;
    }
    const ComponentID cid = components::get_type_id<T>();
    componentSet[cid] = false;
    ComponentStore::get().remove_for<T>(this->id);
  }

  template <typename T, typename... TArgs> T &addComponent(TArgs &&...args) {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("adding component_id:{} {} to entity_id: {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    if (this->has<T>()) {
#if defined(AFTER_HOURS_DEBUG)
      log_warn("This entity {} already has this component attached id: "
               "{}, "
               "component {}",
               id, components::get_type_id<T>(), type_name<T>());
      VALIDATE(false, "duplicate component");
#endif
      // Keep state stable in non-debug builds too (and avoid corrupting
      // any derived/cleanup traversal invariants).
      return this->get<T>();
    }

    const ComponentID cid = components::get_type_id<T>();
    componentSet[cid] = true;
    return ComponentStore::get().emplace_for<T>(this->id,
                                                std::forward<TArgs>(args)...);
  }

  template <typename T, typename... TArgs>
  T &addComponentIfMissing(TArgs &&...args) {
    if (this->has<T>())
      return this->get<T>();
    return addComponent<T>(std::forward<TArgs>(args)...);
  }

  template <typename T> void removeComponentIfExists() {
    if (this->is_missing<T>())
      return;
    return removeComponent<T>();
  }

  template <typename A> void addAll() { addComponent<A>(); }

  template <typename A, typename B, typename... Rest> void addAll() {
    addComponent<A>();
    addAll<B, Rest...>();
  }

  template <typename T> void warnIfMissingComponent() const {
#if defined(AFTER_HOURS_DEBUG)
    if (this->is_missing<T>()) {
      log_warn("This entity {} is missing id: {}, "
               "component {}",
               id, components::get_type_id<T>(), type_name<T>());
    }
#endif
  }

  template <typename T> [[nodiscard]] T &get_with_child() {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("fetching for child components {} {} on entity {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    // Derived-component support:
    // - Walk all *present* component type IDs on this entity (via the bitset)
    // - For each present type ID, fetch the pooled BaseComponent instance
    // - `dynamic_cast` to `T` so callers can ask for a base type and still get a
    //   derived component instance (legacy behavior)
    //
    // NOTE: This path is only used by `get_with_child/has_child_of` (i.e. when
    // systems/queries are operating in "include derived children" mode). The
    // normal `get<T>()` / `has<T>()` path remains O(1).
    T *found = nullptr;
    for_each_component_id([&](const ComponentID cid) {
      BaseComponent *cmp = ComponentStore::get().try_get_base(cid, this->id);
      if (!cmp)
        return bitset_utils::ForEachFlow::Continue;
      if (auto *as_t = dynamic_cast<T *>(cmp)) {
        found = as_t;
        return bitset_utils::ForEachFlow::Break;
      }
      return bitset_utils::ForEachFlow::Continue;
    });
    if (found)
      return *found;
    warnIfMissingComponent<T>();
    return get<T>();
  }

  template <typename T> [[nodiscard]] const T &get_with_child() const {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("fetching for child components {} {} on entity {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    // Const version of the derived-component scan (see non-const overload).
    // We only `dynamic_cast` components that are present per `componentSet`.
    const T *found = nullptr;
    for_each_component_id([&](const ComponentID cid) {
      const BaseComponent *cmp =
          ComponentStore::get().try_get_base(cid, this->id);
      if (!cmp)
        return bitset_utils::ForEachFlow::Continue;
      if (const auto *as_t = dynamic_cast<const T *>(cmp)) {
        found = as_t;
        return bitset_utils::ForEachFlow::Break;
      }
      return bitset_utils::ForEachFlow::Continue;
    });
    if (found)
      return *found;
    return get<T>();
  }

  template <typename T> [[nodiscard]] T &get() {
    warnIfMissingComponent<T>();
    return ComponentStore::get().get_for<T>(this->id);
  }

  template <typename T> [[nodiscard]] const T &get() const {
    warnIfMissingComponent<T>();
    return ComponentStore::get().get_for<T>(this->id);
  }

  void enableTag(const TagId tag_id) {
    if (tag_id >= AFTER_HOURS_MAX_ENTITY_TAGS)
      return;
    tags.set(tag_id);
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  void enableTag(const TEnum tag_enum) {
    enableTag(static_cast<TagId>(tag_enum));
  }

  void disableTag(const TagId tag_id) {
    if (tag_id >= AFTER_HOURS_MAX_ENTITY_TAGS)
      return;
    tags.reset(tag_id);
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  void disableTag(const TEnum tag_enum) {
    disableTag(static_cast<TagId>(tag_enum));
  }

  void setTag(const TagId tag_id, bool enabled) {
    if (enabled) {
      enableTag(tag_id);
    } else {
      disableTag(tag_id);
    }
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  void setTag(const TEnum tag_enum, bool enabled) {
    setTag(static_cast<TagId>(tag_enum), enabled);
  }

  [[nodiscard]] bool hasTag(const TagId tag_id) const {
    if (tag_id >= AFTER_HOURS_MAX_ENTITY_TAGS)
      return false;
    return tags.test(tag_id);
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  [[nodiscard]] bool hasTag(const TEnum tag_enum) const {
    return hasTag(static_cast<TagId>(tag_enum));
  }

  [[nodiscard]] bool hasAllTags(const TagBitset &mask) const {
    return (tags & mask) == mask;
  }

  [[nodiscard]] bool hasAnyTag(const TagBitset &mask) const {
    return (tags & mask).any();
  }

  [[nodiscard]] bool hasNoTags(const TagBitset &mask) const {
    return (tags & mask).none();
  }
};

using RefEntity = std::reference_wrapper<Entity>;
using OptEntityType = std::optional<std::reference_wrapper<Entity>>;

struct OptEntity {
  OptEntityType data;

  OptEntity() {}
  OptEntity(const OptEntityType opt_e) : data(opt_e) {}
  OptEntity(const RefEntity _e) : data(_e) {}
  OptEntity(Entity &_e) : data(_e) {}

  bool has_value() const { return data.has_value(); }
  bool valid() const { return has_value(); }

  Entity *value() { return &(data.value().get()); }
  Entity *operator*() { return value(); }
  Entity *operator->() { return value(); }

  const Entity *value() const { return &(data.value().get()); }
  const Entity *operator*() const { return value(); }
  const Entity *operator->() const { return value(); }

  Entity &asE() { return data.value(); }
  const Entity &asE() const { return data.value(); }

  operator RefEntity() { return data.value(); }
  operator RefEntity() const { return data.value(); }
  operator bool() const { return valid(); }
};

// Treat ECS reference wrappers as "pointer-like" for pointer-free snapshot APIs.
template <> struct is_pointer_like<RefEntity> : std::true_type {};
template <> struct is_pointer_like<OptEntity> : std::true_type {};
} // namespace afterhours
