#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <cstdint>
#include <map>
#include <optional>
#include <type_traits>

#include "../logging.h"
#include "../type_name.h"
#include "base_component.h"

namespace afterhours {
#ifndef AFTER_HOURS_MAX_ENTITY_TAGS
#define AFTER_HOURS_MAX_ENTITY_TAGS 64
#endif

using TagId = std::uint8_t;
using TagBitset = std::bitset<AFTER_HOURS_MAX_ENTITY_TAGS>;
template <typename Base, typename Derived> bool child_of(Derived *derived) {
  return dynamic_cast<Base *>(derived) != nullptr;
}

using ComponentBitSet = std::bitset<max_num_components>;
using ComponentArray =
    std::array<std::unique_ptr<BaseComponent>, max_num_components>;
using EntityID = int;

static std::atomic_int ENTITY_ID_GEN = 0;

struct Entity {
  EntityID id;
  int entity_type = 0;

  ComponentBitSet componentSet;
  ComponentArray componentArray;

  TagBitset tags{};
  bool cleanup = false;

  Entity() : id(ENTITY_ID_GEN++) {}
  Entity(const Entity &) = delete;
  Entity(Entity &&other) noexcept = default;

  virtual ~Entity() {}

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
    for (const auto &component : componentArray) {
      if (child_of<T>(component.get())) {
        return true;
      }
    }
    return false;
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
    componentSet[components::get_type_id<T>()] = false;
    componentArray[components::get_type_id<T>()].reset();
  }

  template <typename T, typename... TArgs> T &addComponent(TArgs &&...args) {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("adding component_id:{} {} to entity_id: {}",
              components::get_type_id<T>(), type_name<T>(), id);

    if (this->has<T>()) {
      log_warn("This entity {} already has this component attached id: "
               "{}, "
               "component {}",
               id, components::get_type_id<T>(), type_name<T>());

      VALIDATE(false, "duplicate component");
    }
#endif

    auto component = std::make_unique<T>(std::forward<TArgs>(args)...);
    const ComponentID component_id = components::get_type_id<T>();
    componentArray[component_id] = std::move(component);
    componentSet[component_id] = true;

#if defined(AFTER_HOURS_DEBUG)
    log_trace("your set is now {}", componentSet);
#endif

    return get<T>();
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
    for (const auto &component : componentArray) {
      if (child_of<T>(component.get())) {
        return static_cast<T &>(*component);
      }
    }
    warnIfMissingComponent<T>();
    return get<T>();
  }

  template <typename T> [[nodiscard]] const T &get_with_child() const {
#if defined(AFTER_HOURS_DEBUG)
    log_trace("fetching for child components {} {} on entity {}",
              components::get_type_id<T>(), type_name<T>(), id);
#endif
    for (const auto &component : componentArray) {
      if (child_of<T>(component.get())) {
        return static_cast<const T &>(*component);
      }
    }
    return get<T>();
  }

  template <typename T> [[nodiscard]] T &get() {
    warnIfMissingComponent<T>();
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
#endif
    return static_cast<T &>(
        *componentArray.at(components::get_type_id<T>()).get());
  }

  template <typename T> [[nodiscard]] const T &get() const {
    warnIfMissingComponent<T>();

    return static_cast<const T &>(
        *componentArray.at(components::get_type_id<T>()).get());
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
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
} // namespace afterhours
