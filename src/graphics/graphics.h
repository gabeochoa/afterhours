#pragma once

#include <filesystem>

#include "graphics_backend.h"

namespace afterhours::graphics {

// ============================================================================
// Internal state (header-only via Meyers singletons)
// ============================================================================

namespace detail {

struct AutoCaptureState {
    int interval = 0;
    std::filesystem::path directory;
    bool enabled = false;
};

inline AutoCaptureState& auto_capture_state() {
    static AutoCaptureState state{};
    return state;
}

inline int& frame_count() {
    static int count = 0;
    return count;
}

inline RenderTextureType& dummy_texture() {
    static RenderTextureType tex{};
    return tex;
}

}  // namespace detail

// ============================================================================
// Lifecycle
// ============================================================================

/// Initialize the graphics backend based on Config.display.
/// @param cfg Configuration specifying display mode, dimensions, and other settings.
/// @return true if initialization succeeded, false otherwise.
inline bool init(const Config& cfg) {
    detail::frame_count() = 0;
    auto& backend = get_backend();
    if (backend.init) {
        return backend.init(cfg);
    }
    return false;
}

/// Shutdown the graphics backend and release all resources.
inline void shutdown() {
    auto& backend = get_backend();
    if (backend.shutdown) {
        backend.shutdown();
    }
    // stop_auto_capture() defined below, call directly to avoid forward decl
    auto& state = detail::auto_capture_state();
    state.enabled = false;
    state.interval = 0;
    state.directory.clear();
}

// ============================================================================
// Frame
// ============================================================================

/// Begin a new frame. Must be called before any rendering operations.
inline void begin_frame() {
    auto& backend = get_backend();
    if (backend.begin_frame) {
        backend.begin_frame();
    }
}

/// Capture the current frame and save it to the specified path.
/// @param path Filesystem path for the output image (PNG format).
/// @return true if capture succeeded, false otherwise.
inline bool capture_frame(const std::filesystem::path& path) {
    auto& backend = get_backend();
    if (backend.capture_frame) {
        return backend.capture_frame(path);
    }
    return false;
}

/// End the current frame. Handles auto-capture if enabled.
inline void end_frame() {
    auto& backend = get_backend();
    if (backend.end_frame) {
        backend.end_frame();
    }

    detail::frame_count()++;

    // Handle auto-capture if enabled
    auto& state = detail::auto_capture_state();
    if (state.enabled && state.interval > 0) {
        if (detail::frame_count() % state.interval == 0) {
            auto filename = "frame_" + std::to_string(detail::frame_count()) + ".png";
            auto path = state.directory / filename;
            capture_frame(path);
        }
    }
}

// ============================================================================
// Capture
// ============================================================================

/// Enable automatic frame capture every N frames.
/// @param n Capture interval (capture every N frames, must be > 0).
/// @param dir Directory to save captured frames.
inline void capture_every_n_frames(int n, const std::filesystem::path& dir) {
    auto& state = detail::auto_capture_state();
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

/// Stop automatic frame capture.
inline void stop_auto_capture() {
    auto& state = detail::auto_capture_state();
    state.enabled = false;
    state.interval = 0;
    state.directory.clear();
}

// ============================================================================
// Timing
// ============================================================================

/// Get the delta time for the current frame.
/// In windowed mode, returns actual frame time.
/// In headless mode, returns simulated time based on target FPS and time scale.
/// @return Delta time in seconds.
inline float get_delta_time() {
    auto& backend = get_backend();
    if (backend.get_delta_time) {
        return backend.get_delta_time();
    }
    return 0.0f;
}

// ============================================================================
// Query
// ============================================================================

/// Check if the graphics backend is running in headless mode.
/// @return true if headless, false if windowed.
inline bool is_headless() {
    auto& backend = get_backend();
    if (backend.is_headless) {
        return backend.is_headless();
    }
    return false;
}

/// Get the current frame count (only meaningful in headless mode).
/// @return Current frame number.
inline int get_frame_count() {
    return detail::frame_count();
}

/// Get a reference to the backend's render texture.
/// In headless mode, this is the offscreen texture used for rendering.
/// In windowed mode, this is typically unused but provided for API consistency.
/// @return Reference to the render texture.
inline RenderTextureType& get_render_texture() {
    auto& backend = get_backend();
    if (backend.get_render_texture) {
        return backend.get_render_texture();
    }
    return detail::dummy_texture();
}

}  // namespace afterhours::graphics

// Auto-include raylib backend when enabled - this ensures registration happens
#ifdef AFTER_HOURS_USE_RAYLIB
#include "raylib/raylib_backend.h"
#endif

// ============================================================================
// Platform API forwarding functions
//
// Application code calls these (e.g. afterhours::graphics::init_window(...)).
// They forward to the active backend's static functions selected at compile time.
// ============================================================================

// Include Metal backend header outside any namespace
#if defined(AFTER_HOURS_USE_METAL) && !defined(AFTER_HOURS_USE_RAYLIB)
#include "metal_backend.h"
#endif

namespace afterhours::graphics {

#ifdef AFTER_HOURS_USE_RAYLIB
using PlatformAPI = RaylibPlatformAPI;
#elif defined(AFTER_HOURS_USE_METAL)
using PlatformAPI = MetalPlatformAPI;
#else
#error "No graphics backend defined. Define AFTER_HOURS_USE_RAYLIB or AFTER_HOURS_USE_METAL."
#endif

// ── Constants ──
inline constexpr unsigned int FLAG_WINDOW_RESIZABLE = PlatformAPI::FLAG_WINDOW_RESIZABLE;
inline constexpr int LOG_ERROR = PlatformAPI::LOG_ERROR;
inline constexpr int TEXTURE_FILTER_BILINEAR = PlatformAPI::TEXTURE_FILTER_BILINEAR;

// ── Window lifecycle ──
inline void init_window(int w, int h, const char* title) { PlatformAPI::init_window(w, h, title); }
inline void close_window() { PlatformAPI::close_window(); }
inline bool window_should_close() { return PlatformAPI::window_should_close(); }
inline bool is_window_ready() { return PlatformAPI::is_window_ready(); }
inline bool is_window_fullscreen() { return PlatformAPI::is_window_fullscreen(); }
inline void toggle_fullscreen() { PlatformAPI::toggle_fullscreen(); }
inline void minimize_window() { PlatformAPI::minimize_window(); }

// ── Config ──
inline void set_config_flags(unsigned int flags) { PlatformAPI::set_config_flags(flags); }
inline void set_target_fps(int fps) { PlatformAPI::set_target_fps(fps); }
inline void set_exit_key(int key) { PlatformAPI::set_exit_key(key); }
inline void set_trace_log_level(int level) { PlatformAPI::set_trace_log_level(level); }

// ── Frame ──
inline void begin_drawing() { PlatformAPI::begin_drawing(); }
inline void end_drawing() { PlatformAPI::end_drawing(); }
inline void clear_background(::afterhours::ColorLike auto c) { PlatformAPI::clear_background(c); }

// ── Screen / timing ──
inline int get_screen_width() { return PlatformAPI::get_screen_width(); }
inline int get_screen_height() { return PlatformAPI::get_screen_height(); }
inline float get_frame_time() { return PlatformAPI::get_frame_time(); }
inline float get_fps() { return PlatformAPI::get_fps(); }
inline double get_time() { return PlatformAPI::get_time(); }

// ── Text measurement ──
inline int measure_text(const char* text, int fontSize) { return PlatformAPI::measure_text(text, fontSize); }

// ── Screenshots ──
inline void take_screenshot(const char* fileName) { PlatformAPI::take_screenshot(fileName); }

// ── Input ──
inline bool is_key_pressed_repeat(int key) { return PlatformAPI::is_key_pressed_repeat(key); }

// ── Application control ──
inline void request_quit() { PlatformAPI::request_quit(); }

// ── Unified run loop (preferred API) ──
// Owns the full lifecycle: window creation, event loop, and teardown.
// Works identically across all backends.
// The legacy init_window/window_should_close/close_window API still works
// under raylib for backwards compatibility.
inline void run(const RunConfig& cfg) { PlatformAPI::run(cfg); }

}  // namespace afterhours::graphics
