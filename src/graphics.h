#pragma once

#include "config.h"
#include "graphics_common.h"
#include "window_helpers.h"

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/backend.h"
#elif defined(AFTER_HOURS_SOKOL)
#include "backends/sokol/backend.h"
#endif

#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
#include "plugins/e2e_testing/test_input.h"
#endif

namespace afterhours::graphics {

#ifdef AFTER_HOURS_USE_RAYLIB
using PlatformAPI = RaylibPlatformAPI;
#elif defined(AFTER_HOURS_SOKOL)
using PlatformAPI = MetalPlatformAPI;
#else
#error                                                                         \
    "No graphics backend defined. Define AFTER_HOURS_USE_RAYLIB, AFTER_HOURS_USE_SOKOL, AFTER_HOURS_USE_METAL, or AFTER_HOURS_USE_VULKAN (AFTER_HOURS_USE_NONE and AFTER_HOURS_USE_PSX have no graphics API)."
#endif

using Camera2D = PlatformAPI::Camera2D;

inline constexpr unsigned int FLAG_WINDOW_RESIZABLE =
    PlatformAPI::FLAG_WINDOW_RESIZABLE;
inline constexpr unsigned int FLAG_VSYNC_HINT = PlatformAPI::FLAG_VSYNC_HINT;
inline constexpr int LOG_ERROR = PlatformAPI::LOG_ERROR;
inline constexpr int TEXTURE_FILTER_BILINEAR =
    PlatformAPI::TEXTURE_FILTER_BILINEAR;
inline constexpr int TEXTURE_FILTER_POINT = PlatformAPI::TEXTURE_FILTER_POINT;
inline constexpr int SHADER_UNIFORM_VEC2 = PlatformAPI::SHADER_UNIFORM_VEC2;
inline constexpr int SHADER_UNIFORM_FLOAT = PlatformAPI::SHADER_UNIFORM_FLOAT;
inline constexpr int SHADER_UNIFORM_VEC4 = PlatformAPI::SHADER_UNIFORM_VEC4;
inline constexpr int SHADER_UNIFORM_VEC3 = PlatformAPI::SHADER_UNIFORM_VEC3;

inline void init_window(int w, int h, const char *title) {
  PlatformAPI::init_window(w, h, title);
}
inline void close_window() { PlatformAPI::close_window(); }
inline bool window_should_close() { return PlatformAPI::window_should_close(); }
inline bool is_window_ready() { return PlatformAPI::is_window_ready(); }
inline bool is_window_fullscreen() {
  return PlatformAPI::is_window_fullscreen();
}
inline bool is_window_focused() { return PlatformAPI::is_window_focused(); }
inline void toggle_fullscreen() { PlatformAPI::toggle_fullscreen(); }
inline void minimize_window() { PlatformAPI::minimize_window(); }
inline void set_window_size(int w, int h) { PlatformAPI::set_window_size(w, h); }
inline void set_window_min_size(int w, int h) {
  PlatformAPI::set_window_min_size(w, h);
}
inline bool window_resize_pending() {
  return PlatformAPI::window_resize_pending();
}
inline void set_window_state(unsigned int flags) {
  PlatformAPI::set_window_state(flags);
}
inline void clear_window_state(unsigned int flags) {
  PlatformAPI::clear_window_state(flags);
}
inline unsigned int window_state() { return PlatformAPI::window_state(); }

inline CursorMode get_cursor_mode() { return cursor_mode_state(); }

inline void set_cursor_mode(CursorMode mode) {
  if (cursor_mode_state() == mode) return;
  cursor_mode_state() = mode;
  PlatformAPI::set_cursor_mode(mode);
}

inline void set_config_flags(unsigned int flags) {
  PlatformAPI::set_config_flags(flags);
}
inline void set_target_fps(int fps) { PlatformAPI::set_target_fps(fps); }
inline void set_exit_key(int key) { PlatformAPI::set_exit_key(key); }
inline void set_trace_log_level(int level) {
  PlatformAPI::set_trace_log_level(level);
}

inline void begin_drawing() { PlatformAPI::begin_drawing(); }
inline void end_drawing() { PlatformAPI::end_drawing(); }
inline void clear_background(::afterhours::ColorLike auto c) {
  PlatformAPI::clear_background(c);
}

inline int get_screen_width() { return PlatformAPI::get_screen_width(); }
inline int get_screen_height() { return PlatformAPI::get_screen_height(); }
inline float get_frame_time() { return PlatformAPI::get_frame_time(); }
inline float get_fps() { return PlatformAPI::get_fps(); }
inline double get_time() { return PlatformAPI::get_time(); }

inline int measure_text(const char *text, int fontSize) {
  return PlatformAPI::measure_text(text, fontSize);
}

inline void take_screenshot(const char *fileName) {
  PlatformAPI::take_screenshot(fileName);
}
inline void set_render_texture_filter(RenderTextureType &rt, int filter) {
  PlatformAPI::set_render_texture_filter(rt, filter);
}
inline ShaderType load_shader(const char *vsFileName, const char *fsFileName) {
  return PlatformAPI::load_shader(vsFileName, fsFileName);
}
inline void unload_shader(ShaderType &shader) {
  PlatformAPI::unload_shader(shader);
}
inline int get_shader_location(ShaderType &shader, const char *uniformName) {
  return PlatformAPI::get_shader_location(shader, uniformName);
}
inline void set_shader_value(ShaderType &shader, int locIndex, const void *value,
                             int uniformType) {
  PlatformAPI::set_shader_value(shader, locIndex, value, uniformType);
}
inline void begin_shader_mode(ShaderType &shader) {
  PlatformAPI::begin_shader_mode(shader);
}
inline void end_shader_mode() { PlatformAPI::end_shader_mode(); }

#ifdef AFTER_HOURS_ENABLE_E2E_TESTING
inline bool is_key_pressed(int key) {
  return testing::test_input::is_key_pressed(
      key, [](int k) { return PlatformAPI::is_key_pressed(k); });
}
inline bool is_key_down(int key) {
  return testing::test_input::is_key_down(
      key, [](int k) { return PlatformAPI::is_key_down(k); });
}
inline bool is_key_released(int key) {
  return testing::test_input::is_key_released(
      key, [](int k) { return PlatformAPI::is_key_released(k); });
}
inline bool is_key_pressed_repeat(int key) {
  return testing::test_input::is_key_pressed(
      key, [](int k) { return PlatformAPI::is_key_pressed_repeat(k); });
}
inline int get_key_pressed() {
  return testing::test_input::get_key_pressed(
      [] { return PlatformAPI::get_key_pressed(); });
}
#else
inline bool is_key_pressed(int key) { return PlatformAPI::is_key_pressed(key); }
inline bool is_key_down(int key) { return PlatformAPI::is_key_down(key); }
inline bool is_key_released(int key) {
  return PlatformAPI::is_key_released(key);
}
inline bool is_key_pressed_repeat(int key) {
  return PlatformAPI::is_key_pressed_repeat(key);
}
#endif
inline int get_char_pressed() { return PlatformAPI::get_char_pressed(); }
#ifndef AFTER_HOURS_ENABLE_E2E_TESTING
inline int get_key_pressed() { return PlatformAPI::get_key_pressed(); }
#endif

inline bool is_mouse_button_pressed(int btn) {
  return PlatformAPI::is_mouse_button_pressed(btn);
}
inline bool is_mouse_button_down(int btn) {
  return PlatformAPI::is_mouse_button_down(btn);
}
inline bool is_mouse_button_released(int btn) {
  return PlatformAPI::is_mouse_button_released(btn);
}
inline bool is_mouse_button_up(int btn) {
  return PlatformAPI::is_mouse_button_up(btn);
}
inline float get_mouse_wheel_move() {
  return PlatformAPI::get_mouse_wheel_move();
}
inline auto get_mouse_position() { return PlatformAPI::get_mouse_position(); }
inline auto get_screen_to_world_2d(const PlatformAPI::Vec2 &position,
                                   const PlatformAPI::Camera2D &camera) {
  return PlatformAPI::get_screen_to_world_2d(position, camera);
}
inline auto get_world_to_screen_2d(const PlatformAPI::Vec2 &position,
                                   const PlatformAPI::Camera2D &camera) {
  return PlatformAPI::get_world_to_screen_2d(position, camera);
}
inline void begin_mode_2d(const PlatformAPI::Camera2D &camera) {
  PlatformAPI::begin_mode_2d(camera);
}
inline void end_mode_2d() { PlatformAPI::end_mode_2d(); }

inline void request_quit() { PlatformAPI::request_quit(); }

inline void run(const RunConfig &cfg) { PlatformAPI::run(cfg); }

} // namespace afterhours::graphics

#ifdef __EMSCRIPTEN__
#include "graphics/web.h"
#endif
