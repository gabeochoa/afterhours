

#pragma once

#include <bitset>
#include <memory>

#include "base_component.h"
#include "entity.h"

class SystemBase {
public:
  SystemBase() {}
  virtual ~SystemBase() {}
  virtual void for_each(Entity &) = 0;
};

template <typename... Components> struct System : SystemBase {

  void for_each(Entity &entity) {
    if constexpr (sizeof...(Components) > 0) {
      if ((entity.template has<Components>() && ...)) {
        for_each_with(entity, entity.template get<Components>()...);
      }
    } else {
      for_each_with(entity);
    }
  }

  virtual void for_each_with(Entity &, Components &...) = 0;
};

struct SystemManager {
  std::vector<std::unique_ptr<SystemBase>> systems_;

  void register_system(std::unique_ptr<SystemBase> system) {
    systems_.emplace_back(std::move(system));
  }

  void tick(Entities &entities) {
    for (auto &system : systems_) {
      for (std::shared_ptr<Entity> entity : entities) {
        system->for_each(*entity);
      }
    }
  }
};
