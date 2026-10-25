#pragma once

// No window, no GPU: a compile-time backend for headless test builds. The
// window is a remembered size, time is a steady clock, input reads nothing.

#include <chrono>
#include <cmath>
#include <cstring>

#include "../../graphics_common.h"
#include "../../logging.h"

namespace afterhours::graphics {

struct NonePlatformAPI {
  struct color_type {
    unsigned char r, g, b, a;
  };
  struct Vec2 {
    float x, y;
  };
  struct Camera2D {
    Vec2 offset;
    Vec2 target;
    float rotation;
    float zoom;
  };

  // ── Constants (raylib's values, so flags round-trip) ──
  static constexpr unsigned int FLAG_WINDOW_RESIZABLE = 0x00000004;
  static constexpr unsigned int FLAG_VSYNC_HINT = 0x00000040;
  static constexpr unsigned int FLAG_WINDOW_HIGHDPI = 0x00002000;
  static constexpr int LOG_ERROR = 5;
  static constexpr int TEXTURE_FILTER_BILINEAR = 1;
  static constexpr int TEXTURE_FILTER_POINT = 0;
  static constexpr int SHADER_UNIFORM_VEC2 = 1;
  static constexpr int SHADER_UNIFORM_FLOAT = 0;
  static constexpr int SHADER_UNIFORM_VEC4 = 3;

  struct State {
    using Clock = std::chrono::steady_clock;
    int width = 0;
    int height = 0;
    bool open = false;
    bool quit = false;
    Clock::time_point start = Clock::now();
    Clock::time_point last = start;
    float frame_time = 0.f;
    float avg_frame_time = 0.f;
  };
  static State &state() {
    static State s;
    return s;
  }

  // ── Window lifecycle ──
  static void init_window(int w, int h, const char *) {
    state().width = w;
    state().height = h;
    state().open = true;
  }
  static void close_window() { state().open = false; }
  static bool window_should_close() { return state().quit; }
  static bool is_window_ready() { return state().open; }
  static bool is_window_fullscreen() { return false; }
  static void toggle_fullscreen() {}
  static void minimize_window() {}
  static void set_window_size(int w, int h) {
    state().width = w;
    state().height = h;
  }
  static void set_window_min_size(int, int) {}
  static void set_window_state(unsigned int) {}
  static void clear_window_state(unsigned int) {}

  // ── Config ──
  static void set_config_flags(unsigned int) {}
  static void set_target_fps(int) {}
  static void set_exit_key(int) {}
  static void set_trace_log_level(int) {}

  // ── Frame: end_drawing ticks the clock ──
  static void begin_drawing() {}
  static void end_drawing() {
    auto &s = state();
    const auto now = State::Clock::now();
    s.frame_time = std::chrono::duration<float>(now - s.last).count();
    s.last = now;
    s.avg_frame_time = s.avg_frame_time == 0.f
                           ? s.frame_time
                           : s.avg_frame_time +
                                 (s.frame_time - s.avg_frame_time) * 0.1f;
  }
  static void clear_background(afterhours::ColorLike auto) {}

  // ── Screen / timing ──
  static int get_screen_width() { return state().width; }
  static int get_screen_height() { return state().height; }
  static float get_frame_time() { return state().frame_time; }
  static float get_fps() {
    return state().avg_frame_time > 0.f ? 1.f / state().avg_frame_time : 0.f;
  }
  static double get_time() {
    return std::chrono::duration<double>(State::Clock::now() - state().start)
        .count();
  }

  // ── Text: no font atlas, so the same estimate as unmeasurable fonts ──
  static int measure_text(const char *text, int font_size) {
    return text ? static_cast<int>(std::strlen(text)) * font_size / 2 : 0;
  }

  // ── Screenshots / shaders: nothing to read back or compile ──
  static void take_screenshot(const char *) {}
  static void set_render_texture_filter(RenderTextureType &, int) {}
  static ShaderType load_shader(const char *, const char *) { return {}; }
  static void unload_shader(ShaderType &) {}
  static int get_shader_location(ShaderType &, const char *) { return -1; }
  static void set_shader_value(ShaderType &, int, const void *, int) {}
  static void begin_shader_mode(ShaderType &) {}
  static void end_shader_mode() {}

  // ── Input: no devices ──
  static bool is_key_pressed(int) { return false; }
  static bool is_key_down(int) { return false; }
  static bool is_key_released(int) { return false; }
  static bool is_key_pressed_repeat(int) { return false; }
  static int get_char_pressed() { return 0; }
  static bool is_mouse_button_pressed(int) { return false; }
  static bool is_mouse_button_down(int) { return false; }
  static bool is_mouse_button_released(int) { return false; }
  static bool is_mouse_button_up(int) { return true; }
  static float get_mouse_wheel_move() { return 0.f; }
  static Vec2 get_mouse_position() { return {0.f, 0.f}; }
  static Vec2 get_screen_to_world_2d(const Vec2 &p, const Camera2D &c) {
    const float zoom = c.zoom == 0.f ? 1.f : c.zoom;
    return {(p.x - c.offset.x) / zoom + c.target.x,
            (p.y - c.offset.y) / zoom + c.target.y};
  }
  static Vec2 get_world_to_screen_2d(const Vec2 &p, const Camera2D &c) {
    const float zoom = c.zoom == 0.f ? 1.f : c.zoom;
    return {c.offset.x + (p.x - c.target.x) * zoom,
            c.offset.y + (p.y - c.target.y) * zoom};
  }
  static void begin_mode_2d(const Camera2D &) {}
  static void end_mode_2d() {}

  // ── Application control ──
  static void request_quit() { state().quit = true; }

  static bool run(const RunConfig &cfg) {
    state() = State{};
    init_window(cfg.width, cfg.height, cfg.title);
    if (cfg.init)
      cfg.init();
    while (!state().quit)
      if (cfg.frame)
        cfg.frame();
    if (cfg.cleanup)
      cfg.cleanup();
    close_window();
    return true;
  }
};

static_assert(PlatformBackend<NonePlatformAPI>,
              "NonePlatformAPI must satisfy PlatformBackend concept");

} // namespace afterhours::graphics
