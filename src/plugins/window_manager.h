
#pragma once

#include "../base_component.h"
#include "../entity_query.h"
#include "../system.h"
#include "developer.h"

namespace afterhours {
namespace window_manager {

struct Resolution {
  int width;
  int height;
};

struct ProvidesCurrentResolution : public BaseComponent {
  Resolution current_resolution;
  ProvidesCurrentResolution(const Resolution &rez) : current_resolution(rez) {}
  [[nodiscard]] int width() const { return current_resolution.width; }
  [[nodiscard]] int height() const { return current_resolution.height; }
};

struct ProvidesTargetFPS : public BaseComponent {
  int fps;
  ProvidesTargetFPS(int f) : fps(f) {}
};

struct ProvidesAvailableWindowResolutions : BaseComponent {
  std::vector<Resolution> available_resolutions;
  ProvidesAvailableWindowResolutions(const std::vector<Resolution> &rez)
      : available_resolutions(rez) {}
};

void add_singleton_components(
    Entity &entity, const Resolution &rez, int target_fps,
    const std::vector<Resolution> &available_resolutions) {
  entity.addComponent<ProvidesCurrentResolution>(rez);
  entity.addComponent<ProvidesTargetFPS>(target_fps);
  entity.addComponent<ProvidesAvailableWindowResolutions>(
      available_resolutions);
}

void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesCurrentResolution>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ProvidesTargetFPS>>());
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesAvailableWindowResolutions>>());
}

void register_update_systems(SystemManager &) {}

} // namespace window_manager
} // namespace afterhours
