#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && \
    !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>

namespace g = afterhours::graphics;
using afterhours::Color;

static int checks = 0;
static int failures = 0;

static void check(bool condition, const char *message) {
  ++checks;
  if (condition) return;
  ++failures;
  std::fprintf(stderr, "FAIL: %s\n", message);
}

// Left half green, right half blue: known pixels at any size.
static void paint() {
  const float width = static_cast<float>(g::get_screen_width());
  const float height = static_cast<float>(g::get_screen_height());
  afterhours::draw_rectangle(RectangleType{0, 0, width, height},
                             Color{0, 0, 255, 255});
  afterhours::draw_rectangle(RectangleType{0, 0, width / 2, height},
                             Color{0, 255, 0, 255});
}

static void capture_at(int width, int height) {
  g::set_window_size(width, height);
  g::begin_frame();
  paint();
  g::end_frame();
  check(g::get_screen_width() == width && g::get_screen_height() == height,
        "screen dimensions reflect capture size");
  const auto capture = afterhours::capture_render_texture_rgba(g::get_render_texture());
  check(capture.has_value(), "capture succeeds");
  if (!capture) return;
  const auto &pixels = capture->pixels;
  check(capture->width == width && capture->height == height,
        "capture dimensions match requested size");
  check(pixels.size() == static_cast<size_t>(width) * height * 4,
        "capture contains full framebuffer");
  if (pixels.size() != static_cast<size_t>(width) * height * 4) return;
  const size_t left =
      (static_cast<size_t>(height / 2) * width + width / 4) * 4;
  const size_t right =
      (static_cast<size_t>(height / 2) * width + width * 3 / 4) * 4;
  check(pixels[left] < 6 && pixels[left + 1] > 249 && pixels[left + 2] < 6,
        "left half is green");
  check(pixels[right] < 6 && pixels[right + 1] < 6 && pixels[right + 2] > 249,
        "right half is blue");
}

int main() {
  g::Config config;
  config.display = g::DisplayMode::Headless;
  config.width = 800;
  config.height = 600;
  if (!g::init(config)) {
    std::fprintf(stderr, "FAIL: Metal headless initialization failed\n");
    return 1;
  }
  capture_at(800, 600);
  capture_at(1280, 720);
  capture_at(800, 600);
  g::shutdown();
  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
