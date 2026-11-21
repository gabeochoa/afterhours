// web_probe.cpp -- simulated-web TU for the R-144 audit.
//
// A web (Emscripten/GLES3) consumer compiles this shape of code: it includes
// the sokol backend headers and odr-uses the headless-init and capture paths.
// The Makefile compiles this TU with web defines and asserts the object has
// no undefined metal_* / vulkan_* symbols (see web_metal_symbols_test.cpp).
//
// The -DSOKOL_GLES3=1 + -D__EMSCRIPTEN__ pair reproduces the exact macro
// state emcc produces (config.h derives SOKOL_GLES3 for __EMSCRIPTEN__).
// __APPLE__ cannot be undefined here: Apple libc++ availability markup
// requires it, so the probe relies on the explicit SOKOL_GLES3 winning
// (config.h: an explicit SOKOL_* -D always wins). The SOKOL_IMPL TU is
// never part of this probe, so no Apple-only implementation code runs.
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
