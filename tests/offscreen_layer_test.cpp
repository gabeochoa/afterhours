#define FMT_HEADER_ONLY
#include <afterhours/src/graphics.h>
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/offscreen_layer.h>
#include <cassert>
#include <cstdio>
using namespace afterhours;
static int failures = 0;
static void check(bool cond, const char *what) {
  if (!cond) {
    std::printf("  FAIL: %s\n", what);
    ++failures;
  }
}
struct Px {
  int r, g, b, a;
};
static Px px(const RgbaCapture &img, int x, int y) {
  const auto i = static_cast<size_t>(y * img.width + x) * 4;
  return {img.pixels[i], img.pixels[i + 1], img.pixels[i + 2], img.pixels[i + 3]};
}

static bool near(Px p, int r, int g, int b) {
  auto d = [](int a, int e) { return a > e + 12 || a + 12 < e; };
  return !d(p.r, r) && !d(p.g, g) && !d(p.b, b);
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 64;
  config.height = 64;
  assert(graphics::init(config));
  auto outer = load_render_texture(32, 32);
  auto inner = load_render_texture(16, 16);
  begin_texture_mode(outer);
  graphics::clear_background(Color{0, 200, 0, 255});
  {
    TextureModeScope inner_scope(inner);
    graphics::clear_background(Color{0, 0, 220, 255});
  }
  draw_render_texture(inner, 0, 0, Color{255, 255, 255, 255});
  draw_rectangle(RectangleType{24, 24, 4, 4}, Color{240, 220, 0, 255});
  end_texture_mode();
  auto outer_img = capture_render_texture_rgba(outer);
  assert(outer_img);
  check(near(px(*outer_img, 4, 4), 0, 0, 220), "inner content composited into outer at 0,0");
  check(near(px(*outer_img, 20, 4), 0, 200, 0), "outer green survives the nested pass");
  check(near(px(*outer_img, 25, 25), 240, 220, 0), "drawing after inner end resumes outer, not screen");
  OffscreenLayer layer(16, 16);
  layer.begin();
  draw_rectangle(RectangleType{0, 0, 16, 16}, Color{255, 255, 255, 128});
  layer.end();
  auto layer_img = capture_render_texture_rgba(layer.rt);
  assert(layer_img);
  const Px lp = px(*layer_img, 8, 8);
  check(near(lp, 128, 128, 128) && lp.a > 90 && lp.a < 170,
        "translucent draw keeps ~50% alpha in the cleared layer");
  graphics::begin_frame();
  graphics::clear_background(Color{220, 0, 0, 255});
  graphics::end_frame();
  auto &main_rt = graphics::get_render_texture();
  auto main_img = capture_render_texture_rgba(main_rt);
  assert(main_img);
  check(near(px(*main_img, 4, 4), 220, 0, 0), "main frame has no leak from nested targets");
  check(near(px(*main_img, 25, 25), 220, 0, 0), "main frame has no leak from outer marker");
  unload_render_texture(outer);
  unload_render_texture(inner);
  graphics::shutdown();
  if (failures) {
    std::printf("offscreen_layer_test: %d failures\n", failures);
    return 1;
  }
  std::printf("offscreen_layer_test: all passed\n");
  return 0;
}
