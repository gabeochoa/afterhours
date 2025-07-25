
#pragma once

#include <sstream>

#include "../base_component.h"
#include "../developer.h"
#include "../entity_query.h"
#include "../logging.h"
#include "../system.h"

namespace afterhours {

struct window_manager : developer::Plugin {

  struct Resolution {
    int width = 0;
    int height = 0;

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
  static Resolution fetch_current_resolution() {
    auto scale = raylib::GetWindowScaleDPI();
    // generally im pretty confident that this will end up with a nice round
    // number resolution but i havent done any research about it - gabe Jan 2025
    return Resolution{
        .width = static_cast<int>(raylib::GetRenderWidth() / scale.x),
        .height = static_cast<int>(raylib::GetRenderHeight() / scale.y)};
  }

  static Resolution fetch_maximum_resolution() {
    int monitor = raylib::GetCurrentMonitor();
    return Resolution{.width = raylib::GetMonitorWidth(0),
                      .height = raylib::GetMonitorHeight(0)};
  }

  static void set_window_size(int width, int height) {
    raylib::SetWindowSize(width, height);
  }
#else
  static Resolution fetch_maximum_resolution() {
    return Resolution{.width = 1280, .height = 720};
  }
  static Resolution fetch_current_resolution() {
    return Resolution{.width = 1280, .height = 720};
  }
  static void set_window_size(int, int) {}
#endif

  static std::vector<Resolution> fetch_available_resolutions() {
    // These come from the steam hardware survey: jan 5 2025
    std::vector<Resolution> resolutions = {
        Resolution{.width = 1280, .height = 720},
        Resolution{.width = 1280, .height = 800},
        Resolution{.width = 1280, .height = 1024},
        Resolution{.width = 1360, .height = 768},
        Resolution{.width = 1366, .height = 768},
        Resolution{.width = 1440, .height = 900},
        Resolution{.width = 1600, .height = 900},
        Resolution{.width = 1680, .height = 1050},
        Resolution{.width = 1920, .height = 1080},
        Resolution{.width = 1920, .height = 1200},
        Resolution{.width = 2560, .height = 1080},
        Resolution{.width = 2560, .height = 1440},
        Resolution{.width = 2560, .height = 1600},
        Resolution{.width = 2880, .height = 1800},
        Resolution{.width = 3440, .height = 1440},
        Resolution{.width = 3840, .height = 2160},
        Resolution{.width = 5120, .height = 1440},
    };

    Resolution max = fetch_maximum_resolution();

    // Filter out resolutions that exceed the maximum supported resolution
    resolutions.erase(std::remove_if(resolutions.begin(), resolutions.end(),
                                     [&](const Resolution &res) {
                                       return res.width > max.width ||
                                              res.height > max.height;
                                     }),
                      resolutions.end());

    return resolutions;
  }

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
        set_window_size(pCR.current_resolution.width,
                        pCR.current_resolution.height);
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

    [[nodiscard]] const std::vector<Resolution> &fetch_data() const {
      return available_resolutions;
    }

    [[nodiscard]] size_t current_index() const {
      const auto &entity = EntityQuery()
                               .whereHasComponent<ProvidesCurrentResolution>()
                               .gen_first_enforce();
      const ProvidesCurrentResolution &pcr =
          entity.get<ProvidesCurrentResolution>();

      for (size_t i = 0; i < available_resolutions.size(); i++) {
        if (pcr.current_resolution == available_resolutions[i]) {
          return i;
        }
      }

      // If current resolution is not in the list, find the closest match
      size_t closest_index = 0;
      int min_diff = std::numeric_limits<int>::max();

      for (size_t i = 0; i < available_resolutions.size(); i++) {
        int diff = std::abs(pcr.current_resolution.width -
                            available_resolutions[i].width) +
                   std::abs(pcr.current_resolution.height -
                            available_resolutions[i].height);
        if (diff < min_diff) {
          min_diff = diff;
          closest_index = i;
        }
      }

      log_once_per(
          std::chrono::minutes(1), VENDOR_LOG_WARN,
          "Could not find the current resolution {} as an available "
          "resolution, using closest match {}",
          static_cast<std::string>(pcr.current_resolution).c_str(),
          static_cast<std::string>(available_resolutions[closest_index])
              .c_str());

      return closest_index;
    }

    Resolution on_data_changed(size_t index) {
      ProvidesCurrentResolution &pcr =
          *(EntityHelper::get_singleton_cmp<ProvidesCurrentResolution>());
      pcr.current_resolution = available_resolutions[index];
      set_window_size(pcr.current_resolution.width,
                      pcr.current_resolution.height);
      return pcr.current_resolution;
    }
  };

  struct CollectAvailableResolutions
      : System<ProvidesAvailableWindowResolutions> {

    virtual void for_each_with(Entity &,
                               ProvidesAvailableWindowResolutions &pAWR,
                               float) override {
      if (pAWR.should_refetch) {
        pAWR.available_resolutions = fetch_available_resolutions();
        pAWR.should_refetch = false;
      }
    }
  };

  static void add_singleton_components(Entity &entity, int target_fps) {
    entity.addComponent<ProvidesTargetFPS>(target_fps);
    entity.addComponent<ProvidesCurrentResolution>();
    entity.addComponent<ProvidesAvailableWindowResolutions>();

    EntityHelper::registerSingleton<ProvidesTargetFPS>(entity);
    EntityHelper::registerSingleton<ProvidesCurrentResolution>(entity);
    EntityHelper::registerSingleton<ProvidesAvailableWindowResolutions>(entity);
  }

  static void add_singleton_components(Entity &entity, const Resolution &rez,
                                       int target_fps) {
    entity.addComponent<ProvidesTargetFPS>(target_fps);
    entity.addComponent<ProvidesCurrentResolution>(rez);
    entity.addComponent<ProvidesAvailableWindowResolutions>();

    EntityHelper::registerSingleton<ProvidesTargetFPS>(entity);
    EntityHelper::registerSingleton<ProvidesCurrentResolution>(entity);
    EntityHelper::registerSingleton<ProvidesAvailableWindowResolutions>(entity);
  }

  static void add_singleton_components(
      Entity &entity, const Resolution &rez, int target_fps,
      const std::vector<Resolution> &available_resolutions) {
    entity.addComponent<ProvidesTargetFPS>(target_fps);
    entity.addComponent<ProvidesCurrentResolution>(rez);
    entity.addComponent<ProvidesAvailableWindowResolutions>(
        available_resolutions);

    EntityHelper::registerSingleton<ProvidesTargetFPS>(entity);
    EntityHelper::registerSingleton<ProvidesCurrentResolution>(entity);
    EntityHelper::registerSingleton<ProvidesAvailableWindowResolutions>(entity);
  }

  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<
            developer::EnforceSingleton<ProvidesCurrentResolution>>());
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesTargetFPS>>());
    sm.register_update_system(
        std::make_unique<
            developer::EnforceSingleton<ProvidesAvailableWindowResolutions>>());
  }

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<CollectCurrentResolution>());
    sm.register_update_system(std::make_unique<CollectAvailableResolutions>());
  }
};
} // namespace afterhours
