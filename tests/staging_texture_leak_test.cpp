#define FMT_HEADER_ONLY
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <cassert>
#include <cstdio>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

static size_t resident_bytes() {
#ifdef __APPLE__
  mach_task_basic_info info{};
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
    return 0;
  return static_cast<size_t>(info.resident_size);
#else
  return 0;
#endif
}

int main() {
  using namespace afterhours;
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 512;
  config.height = 512;
  if (!graphics::init(config)) {
    printf("SKIP: headless Metal init failed (no GPU available)\n");
    return 0;
  }
  auto &rt = graphics::get_render_texture();
#ifdef AFTER_HOURS_SOKOL
  sg_pass clear{};
  clear.attachments.colors[0] = {rt.color_view_id};
  clear.attachments.depth_stencil = {rt.depth_view_id};
  clear.action.colors[0].load_action = SG_LOADACTION_CLEAR;
  clear.action.colors[0].clear_value = {0, 0, 1, 1};
  sg_begin_pass(&clear);
  sg_end_pass();
  sg_commit();
#endif
  graphics::begin_frame();
  draw_rectangle(RectangleType{0, 0, 512, 512}, Color{0, 0, 255, 255});
  graphics::end_frame();

  for (int i = 0; i < 20; i++) {
    auto warm = capture_render_texture_rgba(rt);
    assert(warm && warm->pixels.size() == 512u * 512u * 4u);
  }
  const size_t before = resident_bytes();
  for (int i = 0; i < 200; i++) {
    auto shot = capture_render_texture_rgba(rt);
    assert(shot && shot->pixels.size() == 512u * 512u * 4u);
    assert(shot->pixels[2] == 255);
  }
  const size_t after = resident_bytes();
  const size_t growth = after > before ? after - before : 0;
  printf("resident before=%zu after=%zu growth=%zu bytes\n", before, after,
         growth);
  assert(growth < 64u * 1024u * 1024u);
  graphics::shutdown();
  printf("staging texture leak test passed\n");
  return 0;
}
