
#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <map>
#include <optional>

#include "base_component.h"
#include "logging.h"
#include "type_name.h"

namespace afterhours {
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

  bool cleanup = false;

  Entity() : id(ENTITY_ID_GEN++) {}
  Entity(const Entity &) = delete;
  Entity(Entity &&other) noexcept = default;

  virtual ~Entity() {}

  // These two functions can be used to validate than an entity has all of the
  // matching components that are needed for this system to run
  template <typename T> [[nodiscard]] bool has() const {
    bool result = componentSet[components::get_type_id<T>()];
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

    // checks for duplicates
    if (this->has<T>()) {
      log_warn("This entity {} already has this component attached id: "
               "{}, "
               "component {}",
               id, components::get_type_id<T>(), type_name<T>());

      VALIDATE(false, "duplicate component");
      // Commented out on purpose because the assert is gonna kill the
      // program anyway at some point we should stop enforcing it to avoid
      // crashing the game when people are playing
      //
      // return this->get<T>();
    }
#endif

    auto component = std::make_unique<T>(std::forward<TArgs>(args)...);
    ComponentID component_id = components::get_type_id<T>();
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
};

using RefEntity = std::reference_wrapper<Entity>;
using OptEntityType = std::optional<std::reference_wrapper<Entity>>;

struct OptEntity {
  OptEntityType data;

  OptEntity() {}
  OptEntity(OptEntityType opt_e) : data(opt_e) {}
  OptEntity(RefEntity _e) : data(_e) {}
  OptEntity(Entity &_e) : data(_e) {}

  bool has_value() const { return data.has_value(); }
  bool valid() const { return has_value(); }

  Entity *value() { return &(data.value().get()); }
  Entity *operator*() { return value(); }
  Entity *operator->() { return value(); }

  const Entity *value() const { return &(data.value().get()); }
  const Entity *operator*() const { return value(); }
  // TODO look into switching this to using functional monadic stuff
  // i think optional.transform can take and handle calling functions on an
  // optional without having to expose the underlying value or existance of
  // value
  const Entity *operator->() const { return value(); }

  Entity &asE() { return data.value(); }
  const Entity &asE() const { return data.value(); }

  operator RefEntity() { return data.value(); }
  operator RefEntity() const { return data.value(); }
  operator bool() const { return valid(); }
};
} // namespace afterhours
