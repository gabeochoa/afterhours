

#pragma once

#include <bitset>
#include <memory>

#include "base_component.h"
#include "entity.h"

class SystemBase {
public:
  SystemBase() {}
  virtual ~SystemBase() {}

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
};

template <typename... Components> struct System : SystemBase {

  void for_each(Entity &entity, float dt) {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has<Components>() && ...)) {
        for_each_with(entity, entity.template get<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }

  virtual void for_each_with(Entity &, Components &..., float) {}

  void for_each(const Entity &entity, float dt) const {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has<Components>() && ...)) {
        for_each_with(entity, entity.template get<Components>()..., dt);
      }
    } else {
      for_each_with(entity, dt);
    }
  }
  virtual void for_each_with(const Entity &, const Components &...,
                             float) const {}
};

#include "entity_helper.h"

struct SystemManager {
  std::vector<std::unique_ptr<SystemBase>> update_systems_;
  std::vector<std::unique_ptr<SystemBase>> render_systems_;

  void register_update_system(std::unique_ptr<SystemBase> system) {
    update_systems_.emplace_back(std::move(system));
  }

  void register_render_system(std::unique_ptr<SystemBase> system) {
    render_systems_.emplace_back(std::move(system));
  }

  void tick(Entities &entities, float dt) {
    for (auto &system : update_systems_) {
      if (!system->should_run(dt))
        continue;
      system->once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        system->for_each(*entity, dt);
      }
    }
    EntityHelper::cleanup();
  }

  void render(const Entities &entities, float dt) {
    for (const auto &system : render_systems_) {
      if (!system->should_run(dt))
        continue;
      system->once(dt);
      for (std::shared_ptr<Entity> entity : entities) {
        if (!entity)
          continue;
        const Entity &e = *entity;
        system->for_each(e, dt);
      }
    }
  }

  void tick_all(float dt) {
    auto &entities = EntityHelper::get_entities_for_mod();
    tick(entities, dt);
  }

  void render_all(float dt) {
    const auto &entities = EntityHelper::get_entities();
    render(entities, dt);
  }

  void run(float dt) {
    tick_all(dt);
    render_all(dt);
  }
};
