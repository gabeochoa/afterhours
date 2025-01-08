

#pragma once

#include <bitset>
#include <cmath>
#include <functional>
#include <memory>

#include "base_component.h"
#include "entity.h"

namespace afterhours {
struct SystemBase {
  Entities cache;
  Signature signature;

  SystemBase(const Signature &sig) : signature(sig) {}
  virtual ~SystemBase() {}

  virtual void update_cache(Entities &, const Signature &dirty) = 0;

  // Runs before calling once/for-each, and when
  // false skips calling those
  virtual bool should_run(float) { return true; }
  virtual bool should_run(float) const { return true; }
  // Runs once per frame, before calling for_each
  virtual void once(float) {}
  virtual void once(float) const {}
  // Runs for every entity matching the components
  // in System<Components>
  virtual void for_each(Entity &, float) = 0;
  virtual void for_each(const Entity &, float) const = 0;

  virtual void for_each_cached(float) = 0;
  virtual void for_each_cached(float) const = 0;
};

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

#include "entity_helper.h"

template <typename... Components> struct System : SystemBase {

  System() : SystemBase(get_signature()) {
    // log_info("signature {} {}", typeid(*this).name(), signature);
  }

  static constexpr Signature get_signature() {
    Signature sig;
    ((sig.set(components::get_type_id<Components>())), ...);
    return sig;
  }

  virtual void update_cache(Entities &entities, const Signature &dirty) {
    bool needs_update = (signature & dirty) == signature;
    if (!needs_update)
      return;
    cache.clear();
    for (std::shared_ptr<Entity> entity : entities) {
      if (!entity)
        continue;
      if (entity->has(signature))
        cache.push_back(entity);
    }
    // log_info(" {} cache has {} items", typeid(*this).name(), cache.size());
  }

  virtual void for_each_cached(float dt) override {
    for (std::shared_ptr<Entity> ptr : cache) {
      for_each(*ptr, dt);
    }
  }

  virtual void for_each_cached(float dt) const override {
    for (std::shared_ptr<Entity> ptr : cache) {
      for_each(*ptr, dt);
    }
  }

  virtual void for_each(Entity &entity, float dt) override {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.has(signature))) {
        for_each_with(entity, entity.template get<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

  virtual void for_each(const Entity &entity, float dt) const override {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.has(signature))) {
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
  virtual void for_each_with(Entity &, Components &..., float) {}
  virtual void for_each_with(const Entity &, const Components &...,
                             float) const {}
};

struct CallbackSystem : System<> {
  std::function<void(float)> cb_;
  CallbackSystem(const std::function<void(float)> &cb) : cb_(cb) {}
  virtual void once(float dt) { cb_(dt); }
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

  void update_caches(Entities &entities) {
    for (auto &system : update_systems_) {
      system->update_cache(entities, dirtyComponents);
    }
    for (auto &system : fixed_update_systems_) {
      system->update_cache(entities, dirtyComponents);
    }
    for (auto &system : render_systems_) {
      system->update_cache(entities, dirtyComponents);
    }
    dirtyComponents.reset();
  }

  void tick(Entities &entities, float dt) {
    for (std::unique_ptr<SystemBase> &system_ptr : update_systems_) {
      SystemBase &system = *system_ptr;
      if (!system.should_run(dt))
        continue;
      system.once(dt);
      system.for_each_cached(dt);
      EntityHelper::merge_entity_arrays();
      continue;
    }
  }

  void fixed_tick(Entities &entities, float dt) {
    for (std::unique_ptr<SystemBase> &system_ptr : fixed_update_systems_) {
      SystemBase &system = *system_ptr;
      if (!system.should_run(dt))
        continue;
      system.once(dt);
      system.for_each_cached(dt);
      EntityHelper::merge_entity_arrays();
      continue;
    }
  }

  void render(const Entities &entities, float dt) {
    for (const std::unique_ptr<SystemBase> &system_ptr : render_systems_) {
      SystemBase &system = *system_ptr;
      if (!system.should_run(dt))
        continue;
      system.once(dt);
      const_cast<const SystemBase &>(system).for_each_cached(dt);
      continue;
    }
  }

  void tick_all(Entities &entities, float dt) { tick(entities, dt); }

  void fixed_tick_all(Entities &entities, float dt) {
    accumulator += dt;
    int num_ticks = (int)std::floor(accumulator / FIXED_TICK_RATE);
    accumulator -= (float)num_ticks * FIXED_TICK_RATE;

    while (num_ticks > 0) {
      fixed_tick(entities, FIXED_TICK_RATE);
      num_ticks--;
    }
  }

  void render_all(float dt) {
    const auto &entities = EntityHelper::get_entities();
    render(entities, dt);
  }

  void run(float dt) {
    auto &entities = EntityHelper::get_entities_for_mod();
    // log_info(" {} entities ", entities.size());

    update_caches(entities);

    fixed_tick_all(entities, dt);
    tick_all(entities, dt);

    EntityHelper::cleanup();

    render_all(dt);
  }
};
} // namespace afterhours
