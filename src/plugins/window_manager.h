
#pragma once

#include "../base_component.h"
#include "../entity_query.h"
#include "../system.h"
#include "developer.h"

namespace afterhours {
namespace window_manager {

struct ProvidesCurrentResolution : public BaseComponent {
  int width;
  int height;
  ProvidesCurrentResolution(int w, int h) : width(w), height(h) {}
};

struct ProvidesTargetFPS : public BaseComponent {
  int fps;
  ProvidesTargetFPS(int f) : fps(f) {}
};

void add_singleton_components(Entity &entity, int width, int height,
                              int target_fps) {
  entity.addComponent<ProvidesCurrentResolution>(width, height);
  entity.addComponent<ProvidesTargetFPS>(target_fps);
}

void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesCurrentResolution>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ProvidesTargetFPS>>());
}

void register_update_systems(SystemManager &) {}

} // namespace window_manager
} // namespace afterhours
