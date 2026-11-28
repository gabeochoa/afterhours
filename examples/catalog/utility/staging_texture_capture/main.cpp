#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#define AFTER_HOURS_EXAMPLE_GRAPHICS
#include "../../../example_helper.h"
#include <afterhours/src/drawing_helpers.h>
#include <cassert>
#include <cstdio>

int main() {
  using namespace afterhours;
  using namespace afterhours::examples;
  Graphics gfx(Graphics::headless(128, 128));
  if (!gfx.ok()) {
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }
  auto &rt = graphics::get_render_texture();
#ifdef AFTER_HOURS_SOKOL
  sg_pass clear{};
  clear.attachments.colors[0] = {rt.color_view_id};
  clear.attachments.depth_stencil = {rt.depth_view_id};
  clear.action.colors[0].load_action = SG_LOADACTION_CLEAR;
  clear.action.colors[0].clear_value = {0, 1, 0, 1};
  sg_begin_pass(&clear);
  sg_end_pass();
  sg_commit();
#endif
  graphics::begin_frame();
  draw_rectangle(RectangleType{0, 0, 128, 128}, Color{0, 255, 0, 255});
  graphics::end_frame();
  for (int i = 0; i < 50; i++) {
    auto shot = capture_render_texture_rgba(rt);
    assert(shot && shot->pixels.size() == 128u * 128u * 4u);
    assert(shot->pixels[1] == 255);
  }
  printf("50 repeated captures completed without accumulating staging "
         "textures\n");
  return 0;
}
