// vendor/afterhours/src/graphics/raylib/raylib_backend.h
// Raylib backend implementation - header-only
//
// Include this header in exactly one translation unit to register the raylib backend.
// Alternatively, call afterhours::graphics::raylib::ensure_registered() before init().

#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB

#include "../graphics_backend.h"
#include "raylib_headless.h"
#include "raylib_windowed.h"

#include <variant>

namespace afterhours::graphics {

namespace raylib_backend {

// Backend storage using std::variant for runtime selection
using Backend = std::variant<std::monostate, RaylibWindowed, RaylibHeadless>;

inline Backend& storage() {
    static Backend backend;
    return backend;
}

inline bool raylib_init(const Config& cfg) {
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
        [](auto& backend) {
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
        [](auto& backend) {
            using T = std::decay_t<decltype(backend)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                backend.begin_frame();
            }
        },
        storage());
}

inline void raylib_end_frame() {
    std::visit(
        [](auto& backend) {
            using T = std::decay_t<decltype(backend)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                backend.end_frame();
            }
        },
        storage());
}

inline bool raylib_capture_frame(const std::filesystem::path& path) {
    return std::visit(
        [&path](auto& backend) -> bool {
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
        [](auto& backend) -> float {
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
        [](auto& backend) -> bool {
            using T = std::decay_t<decltype(backend)>;
            if constexpr (!std::is_same_v<T, std::monostate>) {
                return backend.is_headless();
            }
            return false;
        },
        storage());
}

inline RenderTextureType& raylib_get_render_texture() {
    static RenderTextureType dummy{};
    return std::visit(
        [](auto& backend) -> RenderTextureType& {
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
// This ensures registration happens before main() in any TU that includes this header
namespace detail {
struct AutoRegister {
    AutoRegister() { ensure_registered(); }
};
inline AutoRegister auto_register{};
}  // namespace detail

}  // namespace raylib_backend

}  // namespace afterhours::graphics

#endif  // AFTER_HOURS_USE_RAYLIB
