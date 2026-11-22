#ifndef AFTER_HOURS_USE_SOKOL
#define AFTER_HOURS_USE_SOKOL 1
#endif
#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <filesystem>

bool web_probe_capture() {
  afterhours::graphics::RenderTextureType rt{};
  rt.color_img_id = 1;
  rt.width = 4;
  rt.height = 4;
  return afterhours::capture_render_texture(rt,
                                             std::filesystem::path("o.png"));
}

bool web_probe_headless_device() {
  afterhours::graphics::Config cfg{};
  cfg.display = afterhours::graphics::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  return afterhours::graphics::metal_backend::metal_init(cfg);
}
