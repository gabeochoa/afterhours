// draw_triangle must paint either winding on every backend: raylib's
// DrawTriangle culls one order, so the wrapper normalizes before it can
// reach the GPU. Both windings are rasterized into the headless target and
// read back; either one coming back empty is the failure.
#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <cassert>
#include <cstdio>

static int covered_pixels(afterhours::graphics::RenderTextureType &rt) {
  auto rgba = afterhours::capture_render_texture_rgba(rt);
  if (!rgba) {
    return -1;
  }
  int covered = 0;
  for (size_t i = 0; i + 3 < rgba->pixels.size(); i += 4) {
    if (rgba->pixels[i] > 100 && rgba->pixels[i + 2] < 100) {
      covered++;
    }
  }
  return covered;
}

int main() {
  using namespace afterhours;
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 32;
  if (!graphics::init(config)) {
    printf("SKIP: headless init failed (no GPU available)\n");
    return 0;
  }
  auto &rt = graphics::get_render_texture();

  // One triangle, both vertex orders, bright red on the same target.
  const Vector2Type cw[] = {{4, 4}, {60, 4}, {32, 28}};
  const Vector2Type ccw[] = {{4, 4}, {32, 28}, {60, 4}};

  auto rasterize = [&](const Vector2Type (&v)[3]) {
#ifdef AFTER_HOURS_SOKOL
    sg_pass clear{};
    clear.attachments.colors[0] = {rt.color_view_id};
    clear.attachments.depth_stencil = {rt.depth_view_id};
    clear.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    clear.action.colors[0].clear_value = {0, 0, 0, 0};
    sg_begin_pass(&clear);
    sg_end_pass();
    sg_commit();
#endif
    graphics::begin_frame();
#ifdef AFTER_HOURS_USE_RAYLIB
    graphics::clear_background(Color{0, 0, 0, 0});
#endif
    draw_triangle(v[0], v[1], v[2], Color{255, 0, 0, 255});
    graphics::end_frame();
    return covered_pixels(rt);
  };

  const int cw_px = rasterize(cw);
  const int ccw_px = rasterize(ccw);
  printf("triangle winding: cw=%d px, ccw=%d px\n", cw_px, ccw_px);
  assert(cw_px > 100);
  assert(ccw_px > 100);
  graphics::shutdown();
  printf("triangle_winding_test: ok\n");
  return 0;
}
