#pragma once

#include <concepts>
#include <filesystem>
#include <functional>
#include <string>

namespace afterhours::graphics {
enum class DisplayMode { Windowed, Headless };
} // namespace afterhours::graphics

#ifdef AFTER_HOURS_USE_RAYLIB

#if defined(__has_include)
#if __has_include(<raylib.h>)
namespace raylib {
#include <raylib.h>
#include <rlgl.h>
} // namespace raylib
#elif __has_include("raylib/raylib.h")
namespace raylib {
#include "raylib/raylib.h"
#include "raylib/rlgl.h"
} // namespace raylib
#else
#error "raylib headers not found"
#endif
#else
namespace raylib {
#include <raylib.h>
#include <rlgl.h>
} // namespace raylib
#endif

namespace afterhours::graphics {
using RenderTextureType = raylib::RenderTexture2D;
} // namespace afterhours::graphics

#else // !AFTER_HOURS_USE_RAYLIB

namespace afterhours::graphics {
struct RenderTextureType {};
} // namespace afterhours::graphics

#endif // AFTER_HOURS_USE_RAYLIB

namespace afterhours {

template <typename C>
concept ColorLike = requires(C c) {
  { c.r } -> std::convertible_to<unsigned char>;
  { c.g } -> std::convertible_to<unsigned char>;
  { c.b } -> std::convertible_to<unsigned char>;
  { c.a } -> std::convertible_to<unsigned char>;
};

} // namespace afterhours

namespace afterhours::graphics {

struct Config {
  DisplayMode display = DisplayMode::Windowed;
  int width = 1280;
  int height = 720;
  std::string title = "Afterhours";
  float time_scale = 1.0f;
  bool uncapped_fps = false;
  int target_fps = 60;
};

template <typename T>
concept GraphicsBackend =
    requires(T t, const Config &cfg, const std::filesystem::path &path) {
      { t.init(cfg) } -> std::same_as<bool>;
      { t.shutdown() } -> std::same_as<void>;
      { t.is_headless() } -> std::same_as<bool>;
      { t.begin_frame() } -> std::same_as<void>;
      { t.end_frame() } -> std::same_as<void>;
      { t.capture_frame(path) } -> std::same_as<bool>;
      { t.get_render_texture() } -> std::same_as<RenderTextureType &>;
      { t.get_delta_time() } -> std::same_as<float>;
    };

template <typename T>
concept PlatformBackend = requires {
  // ── Window lifecycle ──
  { T::init_window(int{}, int{}, (const char *){}) } -> std::same_as<void>;
  { T::close_window() } -> std::same_as<void>;
  { T::window_should_close() } -> std::same_as<bool>;
  { T::is_window_ready() } -> std::same_as<bool>;
  { T::is_window_fullscreen() } -> std::same_as<bool>;
  { T::toggle_fullscreen() } -> std::same_as<void>;
  { T::minimize_window() } -> std::same_as<void>;

  // ── Config ──
  { T::set_config_flags(unsigned{}) } -> std::same_as<void>;
  { T::set_target_fps(int{}) } -> std::same_as<void>;
  { T::set_exit_key(int{}) } -> std::same_as<void>;
  { T::set_trace_log_level(int{}) } -> std::same_as<void>;

  // ── Frame ──
  { T::begin_drawing() } -> std::same_as<void>;
  { T::end_drawing() } -> std::same_as<void>;
  requires afterhours::ColorLike<typename T::color_type>;
  {
    T::clear_background(std::declval<typename T::color_type>())
  } -> std::same_as<void>;

  // ── Screen / timing ──
  { T::get_screen_width() } -> std::same_as<int>;
  { T::get_screen_height() } -> std::same_as<int>;
  { T::get_frame_time() } -> std::same_as<float>;
  { T::get_fps() } -> std::same_as<float>;
  { T::get_time() } -> std::same_as<double>;

  // ── Text measurement ──
  { T::measure_text((const char *){}, int{}) } -> std::same_as<int>;

  // ── Screenshots ──
  { T::take_screenshot((const char *){}) } -> std::same_as<void>;

  // ── Input: keyboard ──
  { T::is_key_pressed(int{}) } -> std::same_as<bool>;
  { T::is_key_down(int{}) } -> std::same_as<bool>;
  { T::is_key_released(int{}) } -> std::same_as<bool>;
  { T::is_key_pressed_repeat(int{}) } -> std::same_as<bool>;
  { T::get_char_pressed() } -> std::same_as<int>;

  // ── Input: mouse ──
  { T::is_mouse_button_pressed(int{}) } -> std::same_as<bool>;
  { T::is_mouse_button_down(int{}) } -> std::same_as<bool>;
  { T::is_mouse_button_released(int{}) } -> std::same_as<bool>;
  { T::is_mouse_button_up(int{}) } -> std::same_as<bool>;
  { T::get_mouse_wheel_move() } -> std::same_as<float>;

  // ── Application control ──
  { T::request_quit() } -> std::same_as<void>;
};

struct RunConfig {
  int width = 1280;
  int height = 720;
  const char *title = "Afterhours Replace Me";
  int target_fps = 60;
  unsigned int flags = 0;

  std::function<void()> init = nullptr;
  std::function<void()> frame = nullptr;
  std::function<void()> cleanup = nullptr;
};

struct BackendInterface {
  bool (*init)(const Config &) = nullptr;
  void (*shutdown)() = nullptr;
  void (*begin_frame)() = nullptr;
  void (*end_frame)() = nullptr;
  bool (*capture_frame)(const std::filesystem::path &) = nullptr;
  float (*get_delta_time)() = nullptr;
  bool (*is_headless)() = nullptr;
  RenderTextureType &(*get_render_texture)() = nullptr;
};

namespace detail {
inline BackendInterface &backend_storage() {
  static BackendInterface backend{};
  return backend;
}
} // namespace detail

inline void register_backend(const BackendInterface &backend) {
  detail::backend_storage() = backend;
}

inline const BackendInterface &get_backend() {
  return detail::backend_storage();
}

namespace detail {

struct AutoCaptureState {
  int interval = 0;
  std::filesystem::path directory;
  bool enabled = false;
};

inline AutoCaptureState &auto_capture_state() {
  static AutoCaptureState state{};
  return state;
}

inline int &frame_count() {
  static int count = 0;
  return count;
}

inline RenderTextureType &dummy_texture() {
  static RenderTextureType tex{};
  return tex;
}

} // namespace detail

inline bool init(const Config &cfg) {
  detail::frame_count() = 0;
  auto &backend = get_backend();
  if (backend.init) {
    return backend.init(cfg);
  }
  return false;
}

inline void shutdown() {
  auto &backend = get_backend();
  if (backend.shutdown) {
    backend.shutdown();
  }
  auto &state = detail::auto_capture_state();
  state.enabled = false;
  state.interval = 0;
  state.directory.clear();
}

inline void begin_frame() {
  auto &backend = get_backend();
  if (backend.begin_frame) {
    backend.begin_frame();
  }
}

inline bool capture_frame(const std::filesystem::path &path) {
  auto &backend = get_backend();
  if (backend.capture_frame) {
    return backend.capture_frame(path);
  }
  return false;
}

inline void end_frame() {
  auto &backend = get_backend();
  if (backend.end_frame) {
    backend.end_frame();
  }
  detail::frame_count()++;
  auto &state = detail::auto_capture_state();
  if (state.enabled && state.interval > 0) {
    if (detail::frame_count() % state.interval == 0) {
      auto filename = "frame_" + std::to_string(detail::frame_count()) + ".png";
      auto path = state.directory / filename;
      capture_frame(path);
    }
  }
}

inline void capture_every_n_frames(int n, const std::filesystem::path &dir) {
  auto &state = detail::auto_capture_state();
  if (n <= 0) {
    state.enabled = false;
    state.interval = 0;
    state.directory.clear();
    return;
  }
  state.interval = n;
  state.directory = dir;
  state.enabled = true;
  std::filesystem::create_directories(dir);
}

inline void stop_auto_capture() {
  auto &state = detail::auto_capture_state();
  state.enabled = false;
  state.interval = 0;
  state.directory.clear();
}

inline float get_delta_time() {
  auto &backend = get_backend();
  if (backend.get_delta_time) {
    return backend.get_delta_time();
  }
  return 0.0f;
}

inline bool is_headless() {
  auto &backend = get_backend();
  if (backend.is_headless) {
    return backend.is_headless();
  }
  return false;
}

inline int get_frame_count() { return detail::frame_count(); }

inline RenderTextureType &get_render_texture() {
  auto &backend = get_backend();
  if (backend.get_render_texture) {
    return backend.get_render_texture();
  }
  return detail::dummy_texture();
}

} // namespace afterhours::graphics
