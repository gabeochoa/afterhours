#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB

#include <filesystem>
#include <string>

#include "../graphics_concept.h"
#include "../platform/headless_gl.h"

namespace afterhours::graphics {

/// Headless graphics backend using HeadlessGL for GPU context without a window.
/// Used for CI/testing and automated screenshot generation.
struct RaylibHeadless {
    HeadlessGL gl{};
    RenderTextureType render_texture{};
    Config config{};
    bool initialized = false;

    /// Initialize the headless backend with the given configuration.
    /// Creates a headless GL context, initializes rlgl, and sets up a render texture.
    /// @return true if initialization succeeded, false on any failure.
    inline bool init(const Config& cfg) {
        // Validate config parameters
        if (cfg.width <= 0 || cfg.height <= 0 || cfg.target_fps <= 0) {
            return false;
        }

        config = cfg;

        // Initialize headless GL context
        if (!gl.init(cfg.width, cfg.height)) {
            return false;
        }

        // Load GL extensions via rlgl
        raylib::rlLoadExtensions(gl.get_proc_address());

        // Initialize rlgl with the specified dimensions
        raylib::rlglInit(cfg.width, cfg.height);
        raylib::rlSetBlendMode(raylib::RL_BLEND_ALPHA);

        // Create render texture for offscreen rendering
        render_texture = raylib::LoadRenderTexture(cfg.width, cfg.height);

        // Check if render texture was created successfully
        if (render_texture.id == 0) {
            raylib::rlglClose();
            gl.shutdown();
            return false;
        }

        initialized = true;
        return true;
    }

    /// Shutdown the backend, releasing the render texture, rlgl, and GL context.
    inline void shutdown() {
        if (initialized) {
            raylib::UnloadRenderTexture(render_texture);
            raylib::rlglClose();
            gl.shutdown();
            initialized = false;
        }
    }

    /// Returns true - this is a headless backend.
    inline bool is_headless() const { return true; }

    /// Returns a simulated delta time based on target FPS and time scale.
    /// In headless mode, we don't have real frame timing, so we compute:
    /// (1.0f / target_fps) * time_scale
    inline float get_delta_time() const {
        return (1.0f / static_cast<float>(config.target_fps)) * config.time_scale;
    }

    /// Begin rendering to the render texture.
    inline void begin_frame() {
        raylib::BeginTextureMode(render_texture);
    }

    /// End rendering to the render texture.
    inline void end_frame() {
        raylib::EndTextureMode();
    }

    /// Clear the background with the specified color.
    inline void clear(raylib::Color color) {
        raylib::ClearBackground(color);
    }

    /// Capture the current frame and export it to a PNG file.
    /// Uses glFlush/glFinish to ensure GPU operations complete before capture.
    /// @param path The filesystem path to save the PNG to.
    /// @return true if the export succeeded, false otherwise.
    inline bool capture_frame(const std::filesystem::path& path) {
        // Validate render texture is initialized
        if (render_texture.id == 0) {
            return false;
        }

        // Ensure GPU operations complete before reading pixels back to CPU.
        // glFlush() ensures all commands are queued to the GPU.
        // glFinish() blocks until all commands complete execution.
        // Both are needed to guarantee the render texture contains final pixel data.
        glFlush();
        glFinish();

        // Get image from texture
        raylib::Image img = raylib::LoadImageFromTexture(render_texture.texture);
        raylib::ImageFlipVertical(&img);

        // Export using raylib (uses stb_image_write internally)
        bool success = raylib::ExportImage(img, path.c_str());

        raylib::UnloadImage(img);
        return success;
    }

    /// Get a reference to the underlying render texture.
    inline RenderTextureType& get_render_texture() {
        return render_texture;
    }
};

// Verify RaylibHeadless satisfies the GraphicsBackend concept
static_assert(GraphicsBackend<RaylibHeadless>,
              "RaylibHeadless must satisfy GraphicsBackend concept");

}  // namespace afterhours::graphics

#endif  // AFTER_HOURS_USE_RAYLIB
