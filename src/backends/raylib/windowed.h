#pragma once

#ifdef AFTER_HOURS_USE_RAYLIB

#include <filesystem>
#include <string>

#include "../../graphics_common.h"
#include "../../logging.h"

namespace afterhours::graphics {

/// Windowed graphics backend using raylib's standard InitWindow flow.
/// Creates an actual window for visual game rendering.
struct RaylibWindowed {
  RenderTextureType render_texture{};
  Config config{};
  bool initialized = false;
  // Logical (design) draw size + supersample factor (render tex = logical*scale).
  int logical_w_ = 0;
  int logical_h_ = 0;
  int render_scale_ = 1;

  /// Initialize the windowed backend with the given configuration.
  /// Creates a resizable window with MSAA and sets up a render texture for
  /// consistent capture.
  /// @return true if initialization succeeded, false if window or render
  /// texture creation failed.
  bool init(const Config &cfg) {
    // Validate config parameters
    if (cfg.width <= 0 || cfg.height <= 0 || cfg.target_fps <= 0) {
      return false;
    }

    config = cfg;
    logical_w_ = cfg.width;
    logical_h_ = cfg.height;
    unsigned int flags = raylib::FLAG_WINDOW_RESIZABLE | cfg.config_flags;
    if (cfg.enable_msaa) {
      flags |= raylib::FLAG_MSAA_4X_HINT;
    }
    if (cfg.hidpi) {
      flags |= raylib::FLAG_WINDOW_HIGHDPI;
    }
    raylib::SetConfigFlags(flags);
    raylib::InitWindow(cfg.width, cfg.height, cfg.title.c_str());

    // Check if window was created successfully
    if (!raylib::IsWindowReady()) {
      return false;
    }

    raylib::SetTargetFPS(cfg.target_fps);

    // Scale 1 (no hidpi / 1x display) == the old behavior exactly.
    render_scale_ = 1;
    if (cfg.hidpi) {
      const float dpi = raylib::GetWindowScaleDPI().x;
      render_scale_ = dpi > 1.0f ? static_cast<int>(dpi + 0.5f) : 1;
      if (render_scale_ < 1)
        render_scale_ = 1;
    }
    graphics::render_scale() = render_scale_;
    render_texture = raylib::LoadRenderTexture(logical_w_ * render_scale_,
                                               logical_h_ * render_scale_);

    // Check if render texture was created successfully
    if (render_texture.id == 0) {
      raylib::CloseWindow();
      return false;
    }

    initialized = true;
    return true;
  }

  /// Shutdown the backend, releasing resources and closing the window.
  void shutdown() {
    if (initialized) {
      raylib::UnloadRenderTexture(render_texture);
      raylib::CloseWindow();
      initialized = false;
    }
  }

  /// Returns false - this is a windowed (non-headless) backend.
  bool is_headless() const { return false; }

  /// Returns the frame time from raylib for delta time calculations.
  float get_delta_time() const { return raylib::GetFrameTime(); }

  // Supersample: keep draws in logical coords via a projection override (not the
  // modelview, so it survives BeginMode2D) rasterizing into the denser texture.
  void begin_frame() {
    raylib::BeginTextureMode(render_texture);
    if (render_scale_ != 1) {
      raylib::rlDrawRenderBatchActive();
      raylib::rlMatrixMode(RL_PROJECTION);
      raylib::rlLoadIdentity();
      raylib::rlOrtho(0, logical_w_, logical_h_, 0, 0.0, 1.0);
      raylib::rlMatrixMode(RL_MODELVIEW);
      raylib::rlLoadIdentity();
    }
  }

  /// End rendering to the render texture.
  void end_frame() { raylib::EndTextureMode(); }

  /// Clear the background with the specified color.
  void clear(raylib::Color color) { raylib::ClearBackground(color); }

  /// Capture the current frame and export it to a PNG file.
  /// @param path The filesystem path to save the PNG to.
  /// @return true if the export succeeded, false otherwise.
  bool capture_frame(const std::filesystem::path &path) {
    raylib::Image img = raylib::LoadImageFromTexture(render_texture.texture);
    if (img.data == nullptr) { // readback failed (invalid/unloaded texture)
      log_error("capture_frame: texture readback returned a null image "
                "(invalid/unloaded render texture)");
      return false;
    }
    raylib::ImageFlipVertical(&img);
    bool success = raylib::ExportImage(img, path.string().c_str());
    raylib::UnloadImage(img);
    return success;
  }

  /// Get a reference to the underlying render texture.
  RenderTextureType &get_render_texture() { return render_texture; }
};

// Verify RaylibWindowed satisfies the GraphicsBackend concept
static_assert(GraphicsBackend<RaylibWindowed>,
              "RaylibWindowed must satisfy GraphicsBackend concept");

} // namespace afterhours::graphics

#endif // AFTER_HOURS_USE_RAYLIB
