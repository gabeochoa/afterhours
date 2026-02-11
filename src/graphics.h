#pragma once

#include "graphics_common.h"

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/backend.h"
#elif defined(AFTER_HOURS_USE_METAL)
#include "backends/sokol/backend.h"
#endif

namespace afterhours::graphics {

#ifdef AFTER_HOURS_USE_RAYLIB
using PlatformAPI = RaylibPlatformAPI;
#elif defined(AFTER_HOURS_USE_METAL)
using PlatformAPI = MetalPlatformAPI;
#else
#error "No graphics backend defined. Define AFTER_HOURS_USE_RAYLIB or AFTER_HOURS_USE_METAL."
#endif

inline constexpr unsigned int FLAG_WINDOW_RESIZABLE = PlatformAPI::FLAG_WINDOW_RESIZABLE;
inline constexpr int LOG_ERROR = PlatformAPI::LOG_ERROR;
inline constexpr int TEXTURE_FILTER_BILINEAR = PlatformAPI::TEXTURE_FILTER_BILINEAR;

inline void init_window(int w, int h, const char* title) { PlatformAPI::init_window(w, h, title); }
inline void close_window() { PlatformAPI::close_window(); }
inline bool window_should_close() { return PlatformAPI::window_should_close(); }
inline bool is_window_ready() { return PlatformAPI::is_window_ready(); }
inline bool is_window_fullscreen() { return PlatformAPI::is_window_fullscreen(); }
inline void toggle_fullscreen() { PlatformAPI::toggle_fullscreen(); }
inline void minimize_window() { PlatformAPI::minimize_window(); }

inline void set_config_flags(unsigned int flags) { PlatformAPI::set_config_flags(flags); }
inline void set_target_fps(int fps) { PlatformAPI::set_target_fps(fps); }
inline void set_exit_key(int key) { PlatformAPI::set_exit_key(key); }
inline void set_trace_log_level(int level) { PlatformAPI::set_trace_log_level(level); }

inline void begin_drawing() { PlatformAPI::begin_drawing(); }
inline void end_drawing() { PlatformAPI::end_drawing(); }
inline void clear_background(::afterhours::ColorLike auto c) { PlatformAPI::clear_background(c); }

inline int get_screen_width() { return PlatformAPI::get_screen_width(); }
inline int get_screen_height() { return PlatformAPI::get_screen_height(); }
inline float get_frame_time() { return PlatformAPI::get_frame_time(); }
inline float get_fps() { return PlatformAPI::get_fps(); }
inline double get_time() { return PlatformAPI::get_time(); }

inline int measure_text(const char* text, int fontSize) { return PlatformAPI::measure_text(text, fontSize); }

inline void take_screenshot(const char* fileName) { PlatformAPI::take_screenshot(fileName); }

inline bool is_key_pressed(int key) { return PlatformAPI::is_key_pressed(key); }
inline bool is_key_down(int key) { return PlatformAPI::is_key_down(key); }
inline bool is_key_released(int key) { return PlatformAPI::is_key_released(key); }
inline bool is_key_pressed_repeat(int key) { return PlatformAPI::is_key_pressed_repeat(key); }
inline int get_char_pressed() { return PlatformAPI::get_char_pressed(); }

inline bool is_mouse_button_pressed(int btn) { return PlatformAPI::is_mouse_button_pressed(btn); }
inline bool is_mouse_button_down(int btn) { return PlatformAPI::is_mouse_button_down(btn); }
inline bool is_mouse_button_released(int btn) { return PlatformAPI::is_mouse_button_released(btn); }
inline bool is_mouse_button_up(int btn) { return PlatformAPI::is_mouse_button_up(btn); }
inline float get_mouse_wheel_move() { return PlatformAPI::get_mouse_wheel_move(); }
inline auto get_mouse_position() { return PlatformAPI::get_mouse_position(); }

inline void request_quit() { PlatformAPI::request_quit(); }

inline void run(const RunConfig& cfg) { PlatformAPI::run(cfg); }

}  // namespace afterhours::graphics
