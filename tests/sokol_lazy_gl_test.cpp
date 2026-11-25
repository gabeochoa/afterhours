
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>
#include <vector>

namespace g = afterhours::graphics;
using afterhours::Color;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const char *what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what);
  }
}

int main() {
  printf("=== sokol lazy sokol_gl tests ===\n\n");

  g::Config cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = 200;
  cfg.height = 200;
  cfg.title = "lazy gl test";
  if (!g::init(cfg)) {
    fprintf(stderr, "FAIL: headless init failed\n");
    return 1;
  }
  check(!g::metal_detail::g_sgl_ready, "init leaves sokol_gl uninitialised");
  check(g::metal_detail::g_headless_rt.sgl_ctx_id == 0,
        "headless texture has no sgl context before the first draw");

  g::begin_drawing();
  check(g::metal_detail::g_sgl_ready, "first begin_drawing initialises sokol_gl");
  afterhours::draw_rectangle(RectangleType{0, 0, 200, 200}, Color{0, 0, 0, 255});
  afterhours::draw_rectangle(RectangleType{50, 50, 100, 100}, Color{255, 255, 255, 255});
  afterhours::draw_circle(150, 150, 25.0f, Color{255, 255, 255, 255});
  g::end_drawing();

  auto shot = afterhours::capture_render_texture_rgba(g::metal_detail::g_headless_rt);
  check(shot.has_value(), "capture after lazy init succeeds");
  if (shot) {
    const auto &px = shot->pixels;
    const size_t i = (static_cast<size_t>(100) * 200 + 100) * 4;
    check(px.size() == static_cast<size_t>(200) * 200 * 4 && px[i] > 127,
          "white rect drawn after lazy init is lit");
    const size_t j = (static_cast<size_t>(150) * 200 + 150) * 4;
    check(px.size() == static_cast<size_t>(200) * 200 * 4 && px[j] > 127,
          "white circle drawn after lazy init is lit");
  }

  auto child = afterhours::load_render_texture(64, 64);
  check(child.sgl_ctx_id == 0, "load_render_texture defers its sgl context");
  g::begin_frame();
  afterhours::begin_texture_mode(child);
  afterhours::draw_rectangle(RectangleType{0, 0, 64, 64}, Color{255, 255, 255, 255});
  afterhours::end_texture_mode();
  g::end_frame();
  check(child.sgl_ctx_id != 0, "begin_texture_mode creates the sgl context");
  afterhours::unload_render_texture(child);

  g::shutdown();
  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
