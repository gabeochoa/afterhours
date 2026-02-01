#pragma once

#include <filesystem>
#include "graphics_concept.h"

namespace afterhours::graphics {

/// Backend interface - function pointers set by the active backend
struct BackendInterface {
    bool (*init)(const Config&) = nullptr;
    void (*shutdown)() = nullptr;
    void (*begin_frame)() = nullptr;
    void (*end_frame)() = nullptr;
    bool (*capture_frame)(const std::filesystem::path&) = nullptr;
    float (*get_delta_time)() = nullptr;
    bool (*is_headless)() = nullptr;
    RenderTextureType& (*get_render_texture)() = nullptr;
};

namespace detail {
/// Internal storage for the backend - uses Meyers singleton for header-only safety
inline BackendInterface& backend_storage() {
    static BackendInterface backend{};
    return backend;
}
}  // namespace detail

/// Register a backend implementation
inline void register_backend(const BackendInterface& backend) {
    detail::backend_storage() = backend;
}

/// Get the current backend (for internal use)
inline const BackendInterface& get_backend() {
    return detail::backend_storage();
}

}  // namespace afterhours::graphics
