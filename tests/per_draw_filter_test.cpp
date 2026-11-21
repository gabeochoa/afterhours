#include <afterhours/src/graphics.h>
#include <afterhours/src/image.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what);
  }
}

static const std::vector<unsigned char> kPixels = {
    0, 0, 0, 255, 255, 255, 255, 255};

static int draw_and_read(TextureType tex, int filter, int x) {
  auto &rt = graphics::get_render_texture();
  graphics::begin_frame();
  graphics::clear_background(Color{0, 0, 0, 255});
  draw_texture_pro(tex, RectangleType{0, 0, 2, 1},
                   RectangleType{0, 0, 100, 32}, Vector2Type{0, 0}, 0,
                   Color{255, 255, 255, 255}, filter);
  graphics::end_frame();
  auto rgba = capture_render_texture_rgba(rt);
  if (!rgba)
    return -1;
  const auto *p = rgba->pixels.data() + (8 * rgba->width + x) * 4;
  return p[0];
}

int main() {
  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = 100;
  config.height = 32;
  check(graphics::init(config), "headless init");

  TextureType tex = load_texture_from_rgba(kPixels.data(), 2, 1);
  set_texture_filter(tex, graphics::TEXTURE_FILTER_POINT);

  const int point = draw_and_read(tex, graphics::TEXTURE_FILTER_POINT, 20);
  const int bilinear = draw_and_read(tex, graphics::TEXTURE_FILTER_BILINEAR, 20);
  const int def = draw_and_read(tex, -1, 20);
  std::printf("  point=%d bilinear=%d default=%d\n", point, bilinear, def);
  check(point >= 0 && point < 64, "point draw reads near-black");
  check(bilinear > point + 15, "bilinear draw blends toward white");
  check(def == point, "no override matches the texture filter");

  unload_texture(tex);
  graphics::shutdown();
  std::printf("per_draw_filter_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
