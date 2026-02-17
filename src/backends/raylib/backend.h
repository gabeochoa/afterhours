// vendor/afterhours/src/graphics/raylib/raylib_backend.h
// Raylib backend implementation - header-only
//
// Include this header in exactly one translation unit to register the raylib
// backend. Alternatively, call
// afterhours::graphics::raylib::ensure_registered() before init().

#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB

#include "../../graphics_common.h"
#include "headless.h"
#include "windowed.h"

#include <variant>

namespace afterhours::graphics {

namespace raylib_backend {

// Backend storage using std::variant for runtime selection
using Backend = std::variant<std::monostate, RaylibWindowed, RaylibHeadless>;

inline Backend &storage() {
  static Backend backend;
  return backend;
}

inline bool raylib_init(const Config &cfg) {
  if (cfg.display == DisplayMode::Headless) {
    storage() = RaylibHeadless{};
    return std::get<RaylibHeadless>(storage()).init(cfg);
  } else {
    storage() = RaylibWindowed{};
    return std::get<RaylibWindowed>(storage()).init(cfg);
  }
}

inline void raylib_shutdown() {
  std::visit(
      [](auto &backend) {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          backend.shutdown();
        }
      },
      storage());
  storage() = std::monostate{};
}

inline void raylib_begin_frame() {
  std::visit(
      [](auto &backend) {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          backend.begin_frame();
        }
      },
      storage());
}

inline void raylib_end_frame() {
  std::visit(
      [](auto &backend) {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          backend.end_frame();
        }
      },
      storage());
}

inline bool raylib_capture_frame(const std::filesystem::path &path) {
  return std::visit(
      [&path](auto &backend) -> bool {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          return backend.capture_frame(path);
        }
        return false;
      },
      storage());
}

inline float raylib_get_delta_time() {
  return std::visit(
      [](auto &backend) -> float {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          return backend.get_delta_time();
        }
        return 0.0f;
      },
      storage());
}

inline bool raylib_is_headless() {
  return std::visit(
      [](auto &backend) -> bool {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          return backend.is_headless();
        }
        return false;
      },
      storage());
}

inline RenderTextureType &raylib_get_render_texture() {
  static RenderTextureType dummy{};
  return std::visit(
      [](auto &backend) -> RenderTextureType & {
        using T = std::decay_t<decltype(backend)>;
        if constexpr (!std::is_same_v<T, std::monostate>) {
          return backend.get_render_texture();
        }
        return dummy;
      },
      storage());
}

/// Ensure the raylib backend is registered. Call this before graphics::init().
/// Safe to call multiple times.
inline void ensure_registered() {
  static bool registered = false;
  if (!registered) {
    BackendInterface iface{};
    iface.init = raylib_init;
    iface.shutdown = raylib_shutdown;
    iface.begin_frame = raylib_begin_frame;
    iface.end_frame = raylib_end_frame;
    iface.capture_frame = raylib_capture_frame;
    iface.get_delta_time = raylib_get_delta_time;
    iface.is_headless = raylib_is_headless;
    iface.get_render_texture = raylib_get_render_texture;
    register_backend(iface);
    registered = true;
  }
}

// Auto-registration using inline variable with constructor
// This ensures registration happens before main() in any TU that includes this
// header
namespace detail {
struct AutoRegister {
  AutoRegister() { ensure_registered(); }
};
inline AutoRegister auto_register{};
} // namespace detail

} // namespace raylib_backend

struct RaylibPlatformAPI {
  // Lightweight color type satisfying ColorLike — avoids depending on color.h
  struct color_type {
    unsigned char r, g, b, a;
  };

  // ── Constants ──
  static constexpr unsigned int FLAG_WINDOW_RESIZABLE =
      raylib::FLAG_WINDOW_RESIZABLE;
  static constexpr int LOG_ERROR = raylib::LOG_ERROR;
  static constexpr int TEXTURE_FILTER_BILINEAR =
      raylib::TEXTURE_FILTER_BILINEAR;

  // ── Window lifecycle ──
  static void init_window(int w, int h, const char *title) {
    raylib::InitWindow(w, h, title);
  }
  static void close_window() { raylib::CloseWindow(); }
  static bool window_should_close() { return raylib::WindowShouldClose(); }
  static bool is_window_ready() { return raylib::IsWindowReady(); }
  static bool is_window_fullscreen() { return raylib::IsWindowFullscreen(); }
  static void toggle_fullscreen() { raylib::ToggleFullscreen(); }
  static void minimize_window() { raylib::MinimizeWindow(); }

  // ── Config ──
  static void set_config_flags(unsigned int flags) {
    raylib::SetConfigFlags(flags);
  }
  static void set_target_fps(int fps) { raylib::SetTargetFPS(fps); }
  static void set_exit_key(int key) { raylib::SetExitKey(key); }
  static void set_trace_log_level(int level) {
    raylib::SetTraceLogLevel(level);
  }

  // ── Frame ──
  static void begin_drawing() { raylib::BeginDrawing(); }
  static void end_drawing() { raylib::EndDrawing(); }
  static void clear_background(afterhours::ColorLike auto c) {
    raylib::ClearBackground(raylib::Color{c.r, c.g, c.b, c.a});
  }

  // ── Screen / timing ──
  static int get_screen_width() { return raylib::GetScreenWidth(); }
  static int get_screen_height() { return raylib::GetScreenHeight(); }
  static float get_frame_time() { return raylib::GetFrameTime(); }
  static float get_fps() { return static_cast<float>(raylib::GetFPS()); }
  static double get_time() { return raylib::GetTime(); }

  // ── Text measurement ──
  static int measure_text(const char *text, int fontSize) {
    return raylib::MeasureText(text, fontSize);
  }

  // ── Screenshots ──
  static void take_screenshot(const char *fileName) {
    raylib::TakeScreenshot(fileName);
  }

  // ── Input: keyboard ──
  static bool is_key_pressed(int key) { return raylib::IsKeyPressed(key); }
  static bool is_key_down(int key) { return raylib::IsKeyDown(key); }
  static bool is_key_released(int key) { return raylib::IsKeyReleased(key); }
  static bool is_key_pressed_repeat(int key) {
    return raylib::IsKeyPressedRepeat(key);
  }
  static int get_char_pressed() { return raylib::GetCharPressed(); }

  // ── Input: mouse ──
  static bool is_mouse_button_pressed(int btn) {
    return raylib::IsMouseButtonPressed(btn);
  }
  static bool is_mouse_button_down(int btn) {
    return raylib::IsMouseButtonDown(btn);
  }
  static bool is_mouse_button_released(int btn) {
    return raylib::IsMouseButtonReleased(btn);
  }
  static bool is_mouse_button_up(int btn) {
    return raylib::IsMouseButtonUp(btn);
  }
  static float get_mouse_wheel_move() { return raylib::GetMouseWheelMove(); }

  struct Vec2 {
    float x, y;
  };
  static Vec2 get_mouse_position() {
    auto p = raylib::GetMousePosition();
    return {p.x, p.y};
  }

  // ── Application control ──
  static void request_quit() {
    // Raylib doesn't have a direct quit request — closing the window
    // is handled by the OS or by WindowShouldClose() returning true.
    // For the run() loop, we can't force-quit from inside a frame.
    // This is a no-op under the legacy poll-based API.
  }

  // ── Unified run loop ──
  static void run(const RunConfig &cfg) {
    if (cfg.flags)
      set_config_flags(cfg.flags);
    init_window(cfg.width, cfg.height, cfg.title);
    if (cfg.target_fps > 0)
      set_target_fps(cfg.target_fps);
    if (cfg.init)
      cfg.init();
    while (!window_should_close()) {
      if (cfg.frame)
        cfg.frame();
    }
    if (cfg.cleanup)
      cfg.cleanup();
    close_window();
  }
};

static_assert(PlatformBackend<RaylibPlatformAPI>,
              "RaylibPlatformAPI must satisfy PlatformBackend concept");

} // namespace afterhours::graphics

#endif // AFTER_HOURS_USE_RAYLIB
