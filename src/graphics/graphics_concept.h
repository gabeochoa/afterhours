#pragma once

#include <concepts>
#include <filesystem>
#include <string>

#include "graphics_types.h"

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

/// Concept defining the interface all graphics backends must satisfy
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

}  // namespace afterhours::graphics
