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
