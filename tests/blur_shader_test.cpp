// blur_shader_test.cpp
// R-117: BlurPass shipped with no blur.fs anywhere in the tree, so on raylib
// Effect::load("blur") always failed and with_blur drew unblurred. This runs
// the real pass headless over a sharp edge and measures the spread.
//
// Set BLUR_SHOT_DIR to also write before/after PNGs there (for PR evidence).

#define FMT_HEADER_ONLY
#include <afterhours/src/drawing_helpers.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/effects.h>
#include <afterhours/src/plugins/files.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

using namespace afterhours;

static int failures = 0;
static void check(bool ok, const char *what) {
  if (!ok) {
    fprintf(stderr, "  FAIL: %s\n", what);
    failures++;
  }
}

static constexpr int W = 128, H = 64;

static graphics::RenderTextureType sharp_edge() {
  auto rt = load_render_texture(W, H);
  begin_texture_mode(rt);
  graphics::clear_background(Color{0, 0, 0, 255});
  draw_rectangle(RectangleType{W / 2.f, 0.f, W / 2.f, static_cast<float>(H)},
                 Color{255, 255, 255, 255});
  end_texture_mode();
  return rt;
}

static int px(const RgbaCapture &c, int x, int y) {
  return c.pixels[(static_cast<size_t>(y) * c.width + x) * 4];
}

static int max_step(const RgbaCapture &c, int y) {
  int m = 0;
  for (int x = 1; x < c.width; x++)
    m = std::max(m, std::abs(px(c, x, y) - px(c, x - 1, y)));
  return m;
}

static void write_shot(const RgbaCapture &c, const char *name) {
  const char *dir = std::getenv("BLUR_SHOT_DIR");
  if (!dir)
    return;
  std::filesystem::create_directories(dir);
  raylib::Image img{const_cast<uint8_t *>(c.pixels.data()), c.width, c.height, 1,
                    raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  raylib::ImageResizeNN(&img, c.width * 4, c.height * 4);
  raylib::ExportImage(img, (std::filesystem::path(dir) / name).string().c_str());
  raylib::UnloadImage(img);
}

int main() {
  // Effect::load resolves resources/shaders/blur.fs under the repo root.
  if (std::filesystem::exists("../resources/shaders/blur.fs"))
    std::filesystem::current_path("..");
  check(std::filesystem::exists("resources/shaders/blur.fs"),
        "shipped resources/shaders/blur.fs exists");

  graphics::Config config;
  config.display = graphics::DisplayMode::Headless;
  config.width = W;
  config.height = H;
  check(graphics::init(config), "graphics init (headless)");
  files::init("blur_shader_test");

  {
  effects::BlurPass pass;
  check(pass.effect.ok(), "BlurPass loads the shipped blur shader");
  check(pass.effect.location("direction") >= 0, "blur shader has a direction uniform");

  auto frame = sharp_edge();
  auto before = capture_render_texture_rgba(frame);
  check(before.has_value(), "capture before blur");
  const int y = H / 2;
  check(px(*before, W / 2 - 8, y) == 0 && px(*before, W / 2 + 8, y) == 255,
        "source is a sharp black/white edge");
  check(max_step(*before, y) == 255, "source edge is a single 255 step");

  pass.apply(frame, RectangleType{0, 0, W, H}, 6.f);
  end_texture_mode(); // apply() leaves the frame bound for the caller
  auto after = capture_render_texture_rgba(frame);
  check(after.has_value(), "capture after blur");
  printf("  edge: step 255 -> %d, x=edge-4: 0 -> %d, x=edge+4: 255 -> %d, far: %d/%d\n",
         max_step(*after, y), px(*after, W / 2 - 4, y), px(*after, W / 2 + 4, y),
         px(*after, 4, y), px(*after, W - 5, y));
  check(max_step(*after, y) < 128, "blur spreads the edge (max step halved)");
  check(px(*after, W / 2 - 4, y) > 20, "dark side gains light near the edge");
  check(px(*after, W / 2 + 4, y) < 235, "bright side loses light near the edge");
  check(px(*after, 4, y) < 10 && px(*after, W - 5, y) > 245,
        "far from the edge the image is unchanged");
  write_shot(*before, "blur_before.png");
  write_shot(*after, "blur_after.png");
  unload_render_texture(frame);

  auto identity = sharp_edge();
  auto id_before = capture_render_texture_rgba(identity);
  pass.apply(identity, RectangleType{0, 0, W, H}, 0.f);
  end_texture_mode();
  auto id_after = capture_render_texture_rgba(identity);
  check(id_before && id_after && id_before->pixels == id_after->pixels,
        "radius 0 is the identity");
  unload_render_texture(identity);
  } // pass and its render textures die before shutdown

  graphics::shutdown();
  if (failures)
    printf("blur_shader_test: %d FAILURES\n", failures);
  else
    printf("blur_shader_test: all checks passed\n");
  return failures ? 1 : 0;
}
