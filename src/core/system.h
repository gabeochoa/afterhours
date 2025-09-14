
#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <type_traits>

#include "base_component.h"
#include "entity.h"
#include "entity_helper.h"

namespace afterhours {
namespace tags {
template <auto... TagEnums> struct All {};
template <auto... TagEnums> struct Any {};
template <auto... TagEnums> struct None {};
} // namespace tags

template <typename... Ts> struct type_list {};

template <typename List, typename T> struct list_prepend;
template <typename... Ts, typename T> struct list_prepend<type_list<Ts...>, T> {
  using type = type_list<T, Ts...>;
};

template <typename T, typename Enable = void>
struct is_component : std::false_type {};
template <typename T>
struct is_component<T, std::enable_if_t<std::is_base_of_v<BaseComponent, T>>>
    : std::true_type {};

template <typename...> struct filter_components;
template <> struct filter_components<> {
  using type = type_list<>;
};
template <typename T, typename... Rest> struct filter_components<T, Rest...> {
  using prev = typename filter_components<Rest...>::type;
  using type = std::conditional_t<is_component<T>::value,
                                  typename list_prepend<prev, T>::type, prev>;
};
class SystemBase {
public:
  SystemBase() {}
  virtual ~SystemBase() {}

  virtual bool should_run(const float) { return true; }
  virtual bool should_run(const float) const { return true; }
  virtual void once(const float) {}
  virtual void once(const float) const {}
  virtual void after(const float) {}
  virtual void after(const float) const {}
  virtual void for_each(Entity &, const float) = 0;
  virtual void for_each(const Entity &, const float) const = 0;

  bool include_derived_children = false;
  bool ignore_temp_entities = false;

  virtual void for_each_derived(Entity &, const float) = 0;
  virtual void for_each_derived(const Entity &, const float) const = 0;
};

template <typename List> struct SystemForEachBase;
template <typename... Cs> struct SystemForEachBase<type_list<Cs...>> {
  virtual void for_each_with(Entity &, Cs &..., const float) {}
  virtual void for_each_with(const Entity &, const Cs &..., const float) const {
  }
  virtual void for_each_with_derived(Entity &, Cs &..., const float) {}
  virtual void for_each_with_derived(const Entity &, const Cs &...,
                                     const float) const {}
};

template <> struct SystemForEachBase<type_list<>> {
  virtual void for_each_with(Entity &, const float) {}
  virtual void for_each_with(const Entity &, const float) const {}
  virtual void for_each_with_derived(Entity &, const float) {}
  virtual void for_each_with_derived(const Entity &, const float) const {}
};

template <typename... Components>
struct System
    : SystemBase,
      SystemForEachBase<typename filter_components<Components...>::type> {
  using ComponentsOnly = typename filter_components<Components...>::type;
  using ForEachBase = SystemForEachBase<ComponentsOnly>;

  template <typename> struct HasAllComponents;
  template <typename... Cs> struct HasAllComponents<type_list<Cs...>> {
    static bool value(const Entity &entity) {
      return ((entity.template has<Cs>()) && ...);
    }
    static bool value_child(const Entity &entity) {
      return ((entity.template has_child_of<Cs>()) && ...);
    }
  };
#ifdef _WIN32
  template <typename List> struct HasAllComponents {
    static bool value(const Entity &entity) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        return true;
      } else {
        return HasAllComponents<List>::value(entity);
      }
    }
    static bool value_child(const Entity &entity) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        return true;
      } else {
        return HasAllComponents<List>::value_child(entity);
      }
    }
  };
#else
  template <> struct HasAllComponents<type_list<>> {
    static bool value(const Entity &) { return true; }
    static bool value_child(const Entity &) { return true; }
  };
#endif

  template <typename> struct CallWithComponents;
  template <typename... Cs> struct CallWithComponents<type_list<Cs...>> {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      static_cast<ForEachBase *>(self)->for_each_with(
          entity, entity.template get<Cs>()..., dt);
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      static_cast<const ForEachBase *>(self)->for_each_with(
          entity, entity.template get<Cs>()..., dt);
    }
  };
#ifdef _WIN32
  template <typename List> struct CallWithComponents {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        self->for_each_with(entity, dt);
      } else {
        CallWithComponents<List>::call(self, entity, dt);
      }
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        self->for_each_with(entity, dt);
      } else {
        CallWithComponents<List>::call_const(self, entity, dt);
      }
    }
  };
#else
  template <> struct CallWithComponents<type_list<>> {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      self->for_each_with(entity, dt);
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      self->for_each_with(entity, dt);
    }
  };
#endif

  template <typename> struct CallWithChildComponents;
  template <typename... Cs> struct CallWithChildComponents<type_list<Cs...>> {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      static_cast<ForEachBase *>(self)->for_each_with_derived(
          entity, entity.template get_with_child<Cs>()..., dt);
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      static_cast<const ForEachBase *>(self)->for_each_with_derived(
          entity, entity.template get_with_child<Cs>()..., dt);
    }
  };
#ifdef _WIN32
  template <typename List> struct CallWithChildComponents {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        self->for_each_with(entity, dt);
      } else {
        CallWithChildComponents<List>::call(self, entity, dt);
      }
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      if constexpr (std::is_same_v<List, type_list<>>) {
        self->for_each_with(entity, dt);
      } else {
        CallWithChildComponents<List>::call_const(self, entity, dt);
      }
    }
  };
#else
  template <> struct CallWithChildComponents<type_list<>> {
    template <typename Self>
    static void call(Self *self, Entity &entity, const float dt) {
      self->for_each_with(entity, dt);
    }
    template <typename Self>
    static void call_const(const Self *self, const Entity &entity,
                           const float dt) {
      self->for_each_with(entity, dt);
    }
  };
#endif

  template <typename...> struct AllMask {
    static TagBitset value() { return {}; }
  };
  template <auto... Es> struct AllMask<tags::All<Es...>> {
    static TagBitset value() {
      TagBitset m;
      (m.set(static_cast<TagId>(Es)), ...);
      return m;
    }
  };
  template <typename...> struct AnyMask {
    static TagBitset value() { return {}; }
  };
  template <auto... Es> struct AnyMask<tags::Any<Es...>> {
    static TagBitset value() {
      TagBitset m;
      (m.set(static_cast<TagId>(Es)), ...);
      return m;
    }
  };
  template <typename...> struct NoneMask {
    static TagBitset value() { return {}; }
  };
  template <auto... Es> struct NoneMask<tags::None<Es...>> {
    static TagBitset value() {
      TagBitset m;
      (m.set(static_cast<TagId>(Es)), ...);
      return m;
    }
  };

#if __APPLE__
  static TagBitset required_all_mask() {
    static TagBitset m = (TagBitset{} | ... | AllMask<Components>::value());
    return m;
  }

  static TagBitset required_any_mask() {
    static TagBitset m = (TagBitset{} | ... | AnyMask<Components>::value());
    return m;
  }

  static TagBitset forbidden_mask() {
    static TagBitset m = (TagBitset{} | ... | NoneMask<Components>::value());
    return m;
  }

  static bool tags_ok(const Entity &entity) {
    const TagBitset &all = required_all_mask();
    const TagBitset &any = required_any_mask();
    const TagBitset &none = forbidden_mask();
    if (all.any() && !entity.hasAllTags(all))
      return false;
    if (any.any() && !entity.hasAnyTag(any))
      return false;
    if (none.any() && !entity.hasNoTags(none))
      return false;
    return true;
  }

#else
  static bool tags_ok(const Entity &entity) { return true; }
#endif

  void for_each(Entity &entity, const float dt) {
    if (!tags_ok(entity))
      return;
    if (HasAllComponents<ComponentsOnly>::value(entity)) {
      CallWithComponents<ComponentsOnly>::call(this, entity, dt);
    }
  }

  void for_each_derived(Entity &entity, const float dt) {
    if (!tags_ok(entity))
      return;
    if (HasAllComponents<ComponentsOnly>::value_child(entity)) {
      CallWithChildComponents<ComponentsOnly>::call(this, entity, dt);
    }
  }

  void for_each_derived(const Entity &entity, const float dt) const {
    if (!tags_ok(entity))
      return;
    if (HasAllComponents<ComponentsOnly>::value_child(entity)) {
      CallWithChildComponents<ComponentsOnly>::call_const(this, entity, dt);
    }
  }
  virtual void for_each_with_derived(Entity &, Components &..., const float) {}
  virtual void for_each_with_derived(const Entity &, const Components &...,
                                     const float) const {}

  void for_each(const Entity &entity, const float dt) const {
    if (!tags_ok(entity))
      return;
    if (HasAllComponents<ComponentsOnly>::value(entity)) {
      CallWithComponents<ComponentsOnly>::call_const(this, entity, dt);
    }
  }

  using ForEachBase::for_each_with;
  using ForEachBase::for_each_with_derived;
};

struct CallbackSystem : System<> {
  std::function<void(float)> cb_;
  CallbackSystem(const std::function<void(float)> &cb) : cb_(cb) {}
  virtual void once(const float dt) { cb_(dt); }
};

struct SystemManager {
  constexpr static float FIXED_TICK_RATE = 1.f / 120.f;
  float accumulator = 0.f;

  std::vector<std::unique_ptr<SystemBase>> update_systems_;
  std::vector<std::unique_ptr<SystemBase>> fixed_update_systems_;
  std::vector<std::unique_ptr<SystemBase>> render_systems_;

  void register_update_system(std::unique_ptr<SystemBase> system) {
    update_systems_.emplace_back(std::move(system));
  }

  void register_fixed_update_system(std::unique_ptr<SystemBase> system) {
    fixed_update_systems_.emplace_back(std::move(system));
  }

  void register_render_system(std::unique_ptr<SystemBase> system) {
    render_systems_.emplace_back(std::move(system));
  }

  void register_update_system(const std::function<void(float)> &cb) {
    register_update_system(std::make_unique<CallbackSystem>(cb));
  }

  void register_fixed_update_system(const std::function<void(float)> &cb) {
    register_fixed_update_system(std::make_unique<CallbackSystem>(cb));
  }

  void register_render_system(const std::function<void(float)> &cb) {
    register_render_system(std::make_unique<CallbackSystem>(cb));
  }

  void tick(Entities &entities, const float dt) {
    for (auto &system : update_systems_) {
      if (!system->should_run(dt))
        continue;
      system->once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        if (system->include_derived_children)
          system->for_each_derived(*entity, dt);
        else
          system->for_each(*entity, dt);
      }
      system->after(dt);
      EntityHelper::merge_entity_arrays();
    }
  }

  void fixed_tick(Entities &entities, const float dt) {
    for (auto &system : fixed_update_systems_) {
      if (!system->should_run(dt))
        continue;
      system->once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        if (system->include_derived_children)
          system->for_each_derived(*entity, dt);
        else
          system->for_each(*entity, dt);
      }
      system->after(dt);
    }
  }

  void render(const Entities &entities, const float dt) {
    for (const auto &system : render_systems_) {
      if (!system->should_run(dt))
        continue;
      const SystemBase &sys = *system;
      sys.once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        const Entity &e = *entity;
        if (sys.include_derived_children)
          sys.for_each_derived(e, dt);
        else
          sys.for_each(e, dt);
      }
      sys.after(dt);
    }
  }

  void tick_all(Entities &entities, const float dt) { tick(entities, dt); }

  void fixed_tick_all(Entities &entities, const float dt) {
    accumulator += dt;
    int num_ticks = (int)std::floor(accumulator / FIXED_TICK_RATE);
    accumulator -= (float)num_ticks * FIXED_TICK_RATE;

    while (num_ticks > 0) {
      fixed_tick(entities, FIXED_TICK_RATE);
      num_ticks--;
    }
  }

  void render_all(const float dt) {
    const auto &entities = EntityHelper::get_entities();
    render(entities, dt);
  }

  void run(const float dt) {
    auto &entities = EntityHelper::get_entities_for_mod();
    fixed_tick_all(entities, dt);
    tick_all(entities, dt);

    EntityHelper::cleanup();

    render_all(dt);
  }
};
} // namespace afterhours
