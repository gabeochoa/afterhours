
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <sstream>

#include "../core/base_component.h"
#include "../core/entity_query.h"
#include "../core/system.h"
#include "../developer.h"
// Only the backend branches below use graphics:: (metal_detail / is_headless /
// set_window_size). graphics.h #errors without a backend, so guard the include
// to keep backend-agnostic consumers (e.g. the layout tests) building.
#if defined(AFTER_HOURS_USE_RAYLIB) || defined(AFTER_HOURS_USE_METAL)
#include "../graphics.h"
#endif
#include "../logging.h"
#include "../warn_once.h"

// Forward declarations for Metal/Sokol backend (defined in sokol_app.h and
// the app's .mm translation unit respectively).
#ifdef AFTER_HOURS_USE_METAL
extern "C" int sapp_width(void);
extern "C" int sapp_height(void);
extern "C" float sapp_dpi_scale(void);
extern "C" void metal_set_window_size(int width, int height);
#endif

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

  // What fetch_current_resolution() reports when the backend has no window to
  // measure -- a headless run, or a call before the window exists. Without it
  // the render size is 0, every layout collapses, and nothing says why.
  // Assign before the first frame to pick a different size.
  inline static Resolution headless_resolution{1280, 720};

#ifdef AFTER_HOURS_USE_RAYLIB
  static Resolution fetch_current_resolution() {
    const auto scale = raylib::GetWindowScaleDPI();
    const float rw =
        static_cast<float>(raylib::GetRenderWidth()) / std::max(1.0f, scale.x);
    const float rh =
        static_cast<float>(raylib::GetRenderHeight()) / std::max(1.0f, scale.y);

    if (rw <= 0.f || rh <= 0.f) {
      warn_once(0, "window reported a {}x{} render size, so layout has "
                   "nothing to work with; using {}. Normal headless -- set "
                   "window_manager::headless_resolution to change it.",
                rw, rh, std::string(headless_resolution));
      return headless_resolution;
    }

    const float target_aspect = 16.0f / 9.0f;
    int width = 0;
    int height = 0;
    if (rw / rh >= target_aspect) {
      height = static_cast<int>(std::round(rh));
      width = static_cast<int>(std::round(rh * target_aspect));
    } else {
      width = static_cast<int>(std::round(rw));
      height = static_cast<int>(std::round(rw / target_aspect));
    }

    return Resolution{.width = width, .height = height};
  }

  static Resolution fetch_maximum_resolution() {
    const int monitor = raylib::GetCurrentMonitor();
    int width = raylib::GetMonitorWidth(monitor);
    int height = raylib::GetMonitorHeight(monitor);
    if (width <= 0 || height <= 0) {
      width = raylib::GetScreenWidth();
      height = raylib::GetScreenHeight();
    }
    if (width <= 0 || height <= 0) {
      width = 1920;
      height = 1080;
    }
    return Resolution{width, height};
  }

  static void set_window_size(const int width, const int height) {
    raylib::SetWindowSize(width, height);
  }
#elif defined(AFTER_HOURS_USE_METAL)
  // Metal/Sokol backend — uses sapp_width()/sapp_height() for current
  // resolution and an extern Obj-C function for resizing.
  static Resolution fetch_current_resolution() {
    // Return logical (CSS) pixel dimensions so the UI works consistently
    // regardless of DPI scale. The metal_detail accessors return the headless
    // offscreen size (dpi=1) when there is no window.
    const float dpi = std::max(0.01f, graphics::metal_detail::dpi_scale());
    const int w = graphics::metal_detail::screen_w();
    const int h = graphics::metal_detail::screen_h();
    if (w <= 0 || h <= 0) {
      warn_once(0, "sokol reported a {}x{} size, so layout has nothing to "
                   "work with; using {}. Set "
                   "window_manager::headless_resolution to change it.",
                w, h, std::string(headless_resolution));
      return headless_resolution;
    }
    return Resolution{
        .width = static_cast<int>(static_cast<float>(w) / dpi),
        .height = static_cast<int>(static_cast<float>(h) / dpi),
    };
  }

  static Resolution fetch_maximum_resolution() {
    // Conservative fallback; a proper implementation would query
    // NSScreen.mainScreen.frame, but this is sufficient for now.
    return Resolution{.width = 3840, .height = 2160};
  }

  // Windowed: Cocoa NSWindow resize (sokol_impl.mm). Headless: no window, so
  // resize the offscreen render target instead (MetalPlatformAPI::set_window_size).
  static void set_window_size(const int width, const int height) {
    if (graphics::is_headless()) {
      graphics::set_window_size(width, height);
      return;
    }
    ::metal_set_window_size(width, height);
  }
#else
  static Resolution fetch_maximum_resolution() {
    return Resolution{.width = 1280, .height = 720};
  }
  static Resolution fetch_current_resolution() {
    return Resolution{.width = 1280, .height = 720};
  }
  static void set_window_size(const int, const int) {}
#endif

  static std::vector<Resolution> fetch_available_resolutions() {
    // These come from the steam hardware survey: jan 5 2025
    static const Resolution kCandidates[] = {
        {1280, 720},  {1280, 800},  {1280, 1024}, {1360, 768},  {1366, 768},
        {1440, 900},  {1600, 900},  {1680, 1050}, {1920, 1080}, {1920, 1200},
        {2560, 1080}, {2560, 1440}, {2560, 1600}, {2880, 1800}, {3440, 1440},
        {3840, 2160}, {5120, 1440},
    };

    const Resolution max = fetch_maximum_resolution();
    std::vector<Resolution> resolutions;
    resolutions.reserve(sizeof(kCandidates) / sizeof(kCandidates[0]));
    for (const Resolution &res : kCandidates) {
      if (res.width <= max.width && res.height <= max.height)
        resolutions.push_back(res);
    }
    if (resolutions.empty())
      resolutions.push_back(Resolution{1280, 720});

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
                               const float) override {
      const Resolution latest = fetch_current_resolution();
      if (pCR.should_refetch || !(latest == pCR.current_resolution)) {
        pCR.current_resolution = latest;
        pCR.should_refetch = false;
      }
    }
  };

  struct ProvidesTargetFPS : public BaseComponent {
    int fps;
    ProvidesTargetFPS(const int f) : fps(f) {}
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
      const auto &entity = EntityQuery({.force_merge = true})
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
        const int diff = std::abs(pcr.current_resolution.width -
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

    Resolution on_data_changed(const size_t index) {
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
                               const float) override {
      if (pAWR.should_refetch) {
        pAWR.available_resolutions = fetch_available_resolutions();
        pAWR.should_refetch = false;
      }
    }
  };

  // Default overload for PluginCore concept compatibility (uses 60 FPS default)
  static void add_singleton_components(Entity &entity) {
    add_singleton_components(entity, 60);
  }

  static void add_singleton_components(Entity &entity, const int target_fps) {
    entity.addComponent<ProvidesTargetFPS>(target_fps);
    entity.addComponent<ProvidesCurrentResolution>();
    entity.addComponent<ProvidesAvailableWindowResolutions>();

    EntityHelper::registerSingleton<ProvidesTargetFPS>(entity);
    EntityHelper::registerSingleton<ProvidesCurrentResolution>(entity);
    EntityHelper::registerSingleton<ProvidesAvailableWindowResolutions>(entity);
  }

  static void add_singleton_components(Entity &entity, const Resolution &rez,
                                       const int target_fps) {
    entity.addComponent<ProvidesTargetFPS>(target_fps);
    entity.addComponent<ProvidesCurrentResolution>(rez);
    entity.addComponent<ProvidesAvailableWindowResolutions>();

    EntityHelper::registerSingleton<ProvidesTargetFPS>(entity);
    EntityHelper::registerSingleton<ProvidesCurrentResolution>(entity);
    EntityHelper::registerSingleton<ProvidesAvailableWindowResolutions>(entity);
  }

  static void add_singleton_components(
      Entity &entity, const Resolution &rez, const int target_fps,
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

// Compile-time verification that window_manager satisfies the PluginCore
// concept
static_assert(developer::PluginCore<window_manager>,
              "window_manager must implement the core plugin interface");

} // namespace afterhours
