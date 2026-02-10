#pragma once

#include <concepts>
#include <filesystem>
#include <functional>
#include <string>

#include "graphics_types.h"

namespace afterhours {

/// A Color is any type with unsigned char r, g, b, a members.
template <typename C>
concept ColorLike = requires(C c) {
    { c.r } -> std::convertible_to<unsigned char>;
    { c.g } -> std::convertible_to<unsigned char>;
    { c.b } -> std::convertible_to<unsigned char>;
    { c.a } -> std::convertible_to<unsigned char>;
};

}  // namespace afterhours

namespace afterhours::graphics {

/// Configuration for graphics backend initialization
struct Config {
    DisplayMode display = DisplayMode::Windowed;

    int width = 1280;
    int height = 720;
    std::string title = "Afterhours";

    // Headless-specific
    float time_scale = 1.0f;    // 10.0 = 10x faster
    bool uncapped_fps = false;  // true = no frame limiting
    int target_fps = 60;        // For delta_time calculation
};

/// Concept for the internal headless/windowed backend split (existing).
/// Validated by RaylibWindowed and RaylibHeadless.
template <typename T>
concept GraphicsBackend =
    requires(T t, const Config& cfg, const std::filesystem::path& path) {
        { t.init(cfg) } -> std::same_as<bool>;
        { t.shutdown() } -> std::same_as<void>;
        { t.is_headless() } -> std::same_as<bool>;
        { t.begin_frame() } -> std::same_as<void>;
        { t.end_frame() } -> std::same_as<void>;
        { t.capture_frame(path) } -> std::same_as<bool>;
        { t.get_render_texture() } -> std::same_as<RenderTextureType&>;
        { t.get_delta_time() } -> std::same_as<float>;
    };

/// Concept for the platform-level API that application code calls.
/// Each backend provides a struct of static functions satisfying this.
/// Validated via static_assert in each backend file.
template <typename T>
concept PlatformBackend = requires {
    // ── Window lifecycle ──
    { T::init_window(int{}, int{}, (const char*){}) } -> std::same_as<void>;
    { T::close_window() }                             -> std::same_as<void>;
    { T::window_should_close() }                      -> std::same_as<bool>;
    { T::is_window_ready() }                           -> std::same_as<bool>;
    { T::is_window_fullscreen() }                      -> std::same_as<bool>;
    { T::toggle_fullscreen() }                         -> std::same_as<void>;
    { T::minimize_window() }                            -> std::same_as<void>;

    // ── Config ──
    { T::set_config_flags(unsigned{}) }                -> std::same_as<void>;
    { T::set_target_fps(int{}) }                       -> std::same_as<void>;
    { T::set_exit_key(int{}) }                         -> std::same_as<void>;
    { T::set_trace_log_level(int{}) }                  -> std::same_as<void>;

    // ── Frame ──
    { T::begin_drawing() }                             -> std::same_as<void>;
    { T::end_drawing() }                               -> std::same_as<void>;
    // clear_background accepts any ColorLike type
    requires afterhours::ColorLike<typename T::color_type>;
    { T::clear_background(std::declval<typename T::color_type>()) } -> std::same_as<void>;

    // ── Screen / timing ──
    { T::get_screen_width() }                          -> std::same_as<int>;
    { T::get_screen_height() }                         -> std::same_as<int>;
    { T::get_frame_time() }                            -> std::same_as<float>;
    { T::get_fps() }                                   -> std::same_as<float>;
    { T::get_time() }                                  -> std::same_as<double>;

    // ── Text measurement ──
    { T::measure_text((const char*){}, int{}) }        -> std::same_as<int>;

    // ── Screenshots ──
    { T::take_screenshot((const char*){}) }            -> std::same_as<void>;

    // ── Input ──
    { T::is_key_pressed_repeat(int{}) }                -> std::same_as<bool>;

    // ── Application control ──
    { T::request_quit() }                               -> std::same_as<void>;
};

/// Configuration for the unified run() entry point.
/// Provides callbacks for init, frame, and cleanup so the backend
/// can own the event loop (required by Sokol/Metal, optional for raylib).
struct RunConfig {
    int width = 1280;
    int height = 720;
    const char* title = "Afterhours Replace Me";
    int target_fps = 60;
    unsigned int flags = 0;

    std::function<void()> init = nullptr;
    std::function<void()> frame = nullptr;
    std::function<void()> cleanup = nullptr;
};

}  // namespace afterhours::graphics