
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

  friend std::ostream &operator<<(std::ostream &os, const Resolution &rez) {
    os << "(" << rez.width << "," << rez.height << ")";
    return os;
  }

  bool operator==(const Resolution &other) const {
    return width == other.width && height == other.height;
  }

  bool operator<(const Resolution &r) const {
    return width * height < r.width * r.height;
  }

  operator std::string() const {
    std::ostringstream oss;
    oss << "(" << width << "," << height << ")";
    return oss.str();
  }
};

#ifdef AFTER_HOURS_USE_RAYLIB
inline std::vector<Resolution> fetch_available_resolutions() {
  int count = 0;
  const GLFWvidmode *modes = glfwGetVideoModes(glfwGetPrimaryMonitor(), &count);
  std::vector<window_manager::Resolution> resolutions;

  for (int i = 0; i < count; i++) {
    resolutions.push_back(
        window_manager::Resolution{modes[i].width, modes[i].height});
  }
  // Sort the vector
  std::sort(resolutions.begin(), resolutions.end());

  // Remove duplicates
  resolutions.erase(std::unique(resolutions.begin(), resolutions.end()),
                    resolutions.end());

  return resolutions;
}
inline Resolution fetch_current_resolution() {
  return Resolution{.width = raylib::GetRenderWidth(),
                    .height = raylib::GetRenderHeight()};
}
#else
inline std::vector<Resolution> fetch_available_resolutions() {
  return std::vector<Resolution>{{
      Resolution{.width = 1280, .height = 720},
      Resolution{.width = 1920, .height = 1080},
  }};
}
inline Resolution fetch_current_resolution() {
  return Resolution{.width = 1280, .height = 720};
}
#endif

struct ProvidesCurrentResolution : public BaseComponent {
  bool should_refetch = true;
  Resolution current_resolution;
  ProvidesCurrentResolution() {}
  ProvidesCurrentResolution(const Resolution &rez) : current_resolution(rez) {
    should_refetch = false;
  }
  [[nodiscard]] int width() const { return current_resolution.width; }
  [[nodiscard]] int height() const { return current_resolution.height; }
};

struct CollectCurrentResolution : System<ProvidesCurrentResolution> {

  virtual void for_each_with(Entity &, ProvidesCurrentResolution &pCR,
                             float) override {
    if (pCR.should_refetch) {
      pCR.current_resolution = fetch_current_resolution();
      pCR.should_refetch = false;
    }
  }
};

struct ProvidesTargetFPS : public BaseComponent {
  int fps;
  ProvidesTargetFPS(int f) : fps(f) {}
};

struct ProvidesAvailableWindowResolutions : BaseComponent {
  bool should_refetch = true;
  std::vector<Resolution> available_resolutions;
  ProvidesAvailableWindowResolutions() {}
  ProvidesAvailableWindowResolutions(const std::vector<Resolution> &rez)
      : available_resolutions(rez) {
    should_refetch = false;
  }

  const std::vector<Resolution> &fetch_data() const {
    return available_resolutions;
  }
};

struct CollectAvailableResolutions
    : System<ProvidesAvailableWindowResolutions> {

  virtual void for_each_with(Entity &, ProvidesAvailableWindowResolutions &pAWR,
                             float) override {
    if (pAWR.should_refetch) {
      pAWR.available_resolutions = fetch_available_resolutions();
      pAWR.should_refetch = false;
    }
  }
};

inline void add_singleton_components(Entity &entity, int target_fps) {
  entity.addComponent<ProvidesTargetFPS>(target_fps);
  entity.addComponent<ProvidesCurrentResolution>();
  entity.addComponent<ProvidesAvailableWindowResolutions>();
}

inline void add_singleton_components(Entity &entity, const Resolution &rez,
                                     int target_fps) {
  entity.addComponent<ProvidesTargetFPS>(target_fps);
  entity.addComponent<ProvidesCurrentResolution>(rez);
  entity.addComponent<ProvidesAvailableWindowResolutions>();
}

inline void
add_singleton_components(Entity &entity, const Resolution &rez, int target_fps,
                         const std::vector<Resolution> &available_resolutions) {
  entity.addComponent<ProvidesTargetFPS>(target_fps);
  entity.addComponent<ProvidesCurrentResolution>(rez);
  entity.addComponent<ProvidesAvailableWindowResolutions>(
      available_resolutions);
}

inline void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesCurrentResolution>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ProvidesTargetFPS>>());
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesAvailableWindowResolutions>>());
}

inline void register_update_systems(SystemManager &sm) {
  sm.register_update_system(std::make_unique<CollectCurrentResolution>());
  sm.register_update_system(std::make_unique<CollectAvailableResolutions>());
}

} // namespace window_manager
} // namespace afterhours
