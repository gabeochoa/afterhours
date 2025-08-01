

#pragma once

#include <bitset>
#include <cmath>
#include <functional>
#include <memory>

#include "base_component.h"
#include "entity.h"
#include "entity_helper.h"

namespace afterhours {
class SystemBase {
public:
  SystemBase() {}
  virtual ~SystemBase() {}

  // Runs before calling once/for-each, and when
  // false skips calling those
  virtual bool should_run(const float) { return true; }
  virtual bool should_run(const float) const { return true; }
  // Runs once per frame, before calling for_each
  virtual void once(const float) {}
  virtual void once(const float) const {}
  // Runs once per frame, after calling for_each
  virtual void after(const float) {}
  virtual void after(const float) const {}
  // Runs for every entity matching the components
  // in System<Components>
  virtual void for_each(Entity &, const float) = 0;
  virtual void for_each(const Entity &, const float) const = 0;

  bool include_derived_children = false;
  bool ignore_temp_entities = false;

#if defined(AFTER_HOURS_INCLUDE_DERIVED_CHILDREN)
  virtual void for_each_derived(Entity &, const float) = 0;
  virtual void for_each_derived(const Entity &, const float) const = 0;
#endif
};

template <typename... Components> struct System : SystemBase {

  /*
   *

   TODO I would like to support the ability to add Not<> Queries to the systesm
to filter out things you dont want

But template meta programming is tough

    System<Not<Transform>> => would run for anything that doesnt have transform
    System<Health, Not<Dead>> => would run for anything that has health and not
dead

template <typename Component> struct Not : BaseComponent {
  using type = Component;
};
  template <typename... Cs> void for_each(Entity &entity, float dt) {
    if constexpr (sizeof...(Cs) > 0) {
      if (
          //
          (
              //
              (std::is_base_of_v<Not<typename Cs::type>, Cs> //
                   ? entity.template is_missing<Cs::type>()
                   : entity.template has<Cs>() //
               ) &&
              ...)
          //
      ) {
        for_each_with(entity,
                      entity.template get<std::conditional_t<
                          std::is_base_of_v<Not<typename Cs::type>, Cs>,
                          typename Cs::type, Cs>>()...,
                      dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

  void for_each(Entity &entity, float dt) {
    for_each_with(entity, entity.template get<Components>()..., dt);
  }
*/
  void for_each(Entity &entity, const float dt) {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has<Components>() && ...)) {
        for_each_with(entity, entity.template get<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

#if defined(AFTER_HOURS_INCLUDE_DERIVED_CHILDREN)
  void for_each_derived(Entity &entity, const float dt) {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has_child_of<Components>() && ...)) {
        for_each_with_derived(
            entity, entity.template get_with_child<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

  void for_each_derived(const Entity &entity, const float dt) const {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has_child_of<Components>() && ...)) {
        for_each_with_derived(
            entity, entity.template get_with_child<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }
  virtual void for_each_with_derived(Entity &, Components &..., const float) {}
  virtual void for_each_with_derived(const Entity &, const Components &...,
                                     const float) const {}
#endif

  void for_each(const Entity &entity, const float dt) const {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has<Components>() && ...)) {
        for_each_with(entity, entity.template get<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

  // Left for the subclass to implment,
  // These would be abstract but we dont know if they will want
  // const or non const versions and the sfinae version is probably
  // pretty unreadable (and idk how to write it correctly)
  virtual void for_each_with(Entity &, Components &..., const float) {}
  virtual void for_each_with(const Entity &, const Components &...,
                             const float) const {}
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

  // TODO  - one issue is that if you write a system that could be const
  // but you add it to update, it wont work since update only calls the
  // non-const for_each_with
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
#if defined(AFTER_HOURS_INCLUDE_DERIVED_CHILDREN)
        if (system->include_derived_children)
          system->for_each_derived(*entity, dt);
        else
#endif
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
#if defined(AFTER_HOURS_INCLUDE_DERIVED_CHILDREN)
        if (system->include_derived_children)
          system->for_each_derived(*entity, dt);
        else
#endif
          system->for_each(*entity, dt);
      }
      system->after(dt);
    }
  }

  void render(const Entities &entities, const float dt) {
    for (const auto &system : render_systems_) {
      if (!system->should_run(dt))
        continue;
      system->once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        const Entity &e = *entity;
#if defined(AFTER_HOURS_INCLUDE_DERIVED_CHILDREN)
        if (system->include_derived_children)
          system->for_each_derived(e, dt);
        else
#endif
          system->for_each(e, dt);
      }
      system->after(dt);
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
