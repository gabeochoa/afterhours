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
// Include EntityHelper before Entity - entity_helper.h forward-declares Entity
// This allows Entity template methods to use EntityHelper when parsed
#include "entity_helper.h"

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
    ComponentID cid = components::get_type_id<T>();
    
    // Check SOA storage first (EntityHelper included at top of file)
    // Template instantiation happens after EntityHelper is fully defined
    bool soa_result = false;
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      // EntityHelper will be defined when this template is instantiated
      soa_result = EntityHelper::get_component_storage<T>().has_component(id);
    }
    
    // Also check componentSet for backward compatibility
    bool aos_result = componentSet[cid];
    
    const bool result = soa_result || aos_result;
#if defined(AFTER_HOURS_DEBUG)
    log_trace("checking component {} {} on entity {}",
              cid, type_name<T>(), id);
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
    
    ComponentID cid = components::get_type_id<T>();
    
    // Remove from SOA storage
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      auto &storage = EntityHelper::get_component_storage<T>();
      if (storage.has_component(id)) {
        storage.remove_component(id);
      }
      
      // Update fingerprint
      ComponentBitSet fingerprint = EntityHelper::get_fingerprint_for_entity(id);
      fingerprint[cid] = false;
      EntityHelper::update_fingerprint_for_entity(id, fingerprint);
    }
    
    // Also update AoS for backward compatibility
    componentSet[cid] = false;
    componentArray[cid].reset();
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

    const ComponentID component_id = components::get_type_id<T>();
    
    // Add to SOA storage first
    T *soa_component = nullptr;
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      auto &storage = EntityHelper::get_component_storage<T>();
      T &soa_ref = storage.add_component(id, std::forward<TArgs>(args)...);
      soa_component = &soa_ref;
      
      // Update fingerprint
      ComponentBitSet fingerprint = EntityHelper::get_fingerprint_for_entity(id);
      fingerprint[component_id] = true;
      EntityHelper::update_fingerprint_for_entity(id, fingerprint);
    }
    
    // Also add to AoS for backward compatibility
    // If SOA component exists, copy from it; otherwise create new
    std::unique_ptr<T> aos_component;
    if (soa_component) {
      aos_component = std::make_unique<T>(*soa_component);
    } else {
      aos_component = std::make_unique<T>(std::forward<TArgs>(args)...);
    }
    componentArray[component_id] = std::move(aos_component);
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
    
    // Try SOA storage first
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      auto *soa_component = EntityHelper::get_component_for_entity<T>(id);
      if (soa_component) {
        return *soa_component;
      }
    }
    
    // Fallback to AoS for backward compatibility
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
#endif
    return static_cast<T &>(
        *componentArray.at(components::get_type_id<T>()).get());
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  }

  template <typename T> [[nodiscard]] const T &get() const {
    warnIfMissingComponent<T>();

    // Try SOA storage first
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      const auto *soa_component = EntityHelper::get_component_for_entity_const<T>(id);
      if (soa_component) {
        return *soa_component;
      }
    }
    
    // Fallback to AoS for backward compatibility
    return static_cast<const T &>(
        *componentArray.at(components::get_type_id<T>()).get());
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

// Implement EntityHelper methods that need Entity to be complete
// These are defined here after Entity is fully defined

inline Entity &EntityHelper::createEntity() {
  return createEntityWithOptions({.is_permanent = false});
}

inline Entity &EntityHelper::createPermanentEntity() {
  return createEntityWithOptions({.is_permanent = true});
}

inline Entity &EntityHelper::createEntityWithOptions(const CreationOptions &options) {
  if (get_temp().capacity() == 0) [[unlikely]]
    reserve_temp_space();

  std::shared_ptr<Entity> e(new Entity());
  get_temp().push_back(e);

  if (options.is_permanent) {
    EntityHelper::get().permanant_ids.insert(e->id);
  }

  return *e;
}

inline void EntityHelper::merge_entity_arrays() {
  if (get_temp().empty())
    return;

  for (const auto &entity : get_temp()) {
    if (!entity)
      continue;
    if (entity->cleanup)
      continue;
    get_entities_for_mod().push_back(entity);
  }
  get_temp().clear();
}

template <typename Component>
inline void EntityHelper::registerSingleton(Entity &ent) {
  const ComponentID id = components::get_type_id<Component>();

  if (EntityHelper::get().singletonMap.contains(id)) {
    log_error("Already had registered singleton {}", type_name<Component>());
  }

  EntityHelper::get().singletonMap.emplace(id, &ent);
  log_info("Registered singleton {} for {} ({})", ent.id,
           type_name<Component>(), id);
}

template <typename Component>
inline RefEntity EntityHelper::get_singleton() {
  const ComponentID id = components::get_type_id<Component>();
  auto &singleton_map = EntityHelper::get().singletonMap;
  if (!singleton_map.contains(id)) {
    log_warn("Singleton map is missing value for component {} ({}). Did you "
             "register this component previously?",
             id, type_name<Component>());
    static Entity dummy_entity;
    return dummy_entity;
  }
  auto *entity_ptr = singleton_map.at(id);
  if (!entity_ptr) {
    log_error("Singleton map contains null pointer for component {} ({})",
              id, type_name<Component>());
    static Entity dummy_entity;
    return dummy_entity;
  }
  return *entity_ptr;
}

template <typename Component>
inline Component *EntityHelper::get_singleton_cmp() {
  Entity &ent = get_singleton<Component>();
  return &(ent.get<Component>());
}

inline void EntityHelper::markIDForCleanup(const int e_id) {
  const auto &entities = get_entities();
  auto it = entities.begin();
  while (it != get_entities().end()) {
    if ((*it)->id == e_id) {
      (*it)->cleanup = true;
      break;
    }
    it++;
  }
}

inline void EntityHelper::cleanup() {
  EntityHelper::merge_entity_arrays();
  Entities &entities = get_entities_for_mod();

  const auto newend = std::remove_if(
      entities.begin(), entities.end(),
      [](const auto &entity) { return !entity || entity->cleanup; });

  entities.erase(newend, entities.end());
}

inline void EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL() {
  Entities &entities = get_entities_for_mod();
  entities.clear();
  EntityHelper::get().temp_entities.clear();
}

inline void EntityHelper::delete_all_entities(const bool include_permanent) {
  EntityHelper::merge_entity_arrays();

  if (include_permanent) {
    delete_all_entities_NO_REALLY_I_MEAN_ALL();
    return;
  }

  Entities &entities = get_entities_for_mod();

  const auto newend = std::remove_if(
      entities.begin(), entities.end(), [](const auto &entity) {
        return !EntityHelper::get().permanant_ids.contains(entity->id);
      });

  entities.erase(newend, entities.end());
}

inline void EntityHelper::forEachEntity(const std::function<ForEachFlow(Entity &)> &cb) {
  for (const auto &e : get_entities()) {
    if (!e)
      continue;
    const auto fef = cb(*e);
    if (fef == 1)
      continue;
    if (fef == 2)
      break;
  }
}

inline std::shared_ptr<Entity> EntityHelper::getEntityAsSharedPtr(const Entity &entity) {
  for (const std::shared_ptr<Entity> &current_entity : get_entities()) {
    if (entity.id == current_entity->id)
      return current_entity;
  }
  return {};
}

inline std::shared_ptr<Entity> EntityHelper::getEntityAsSharedPtr(const OptEntity entity) {
  if (!entity)
    return {};
  const Entity &e = entity.asE();
  return getEntityAsSharedPtr(e);
}

inline OptEntity EntityHelper::getEntityForID(const EntityID id) {
  if (id == -1)
    return {};

  for (const auto &e : get_entities()) {
    if (!e)
      continue;
    if (e->id == id)
      return *e;
  }
  return {};
}

inline Entity &EntityHelper::getEntityForIDEnforce(const EntityID id) {
  auto opt_ent = getEntityForID(id);
  return opt_ent.asE();
}

} // namespace afterhours
