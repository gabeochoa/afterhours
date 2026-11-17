#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cstdio>
#include <cstdlib>

#include <unistd.h>

namespace g = afterhours::graphics;
using afterhours::Color;

static bool returned_from_run = false;

static int checks = 0;
static int failures = 0;

static void check(bool condition, const char *message) {
  ++checks;
  if (condition) return;
  ++failures;
  std::fprintf(stderr, "FAIL: %s\n", message);
}

int main() {
  std::atexit([] {
    if (returned_from_run) return;
    std::fprintf(stderr, "FAIL: the process exited inside run()\n");
    _exit(1);
  });
  int inits = 0, frames = 0, cleanups = 0;
  bool headless_in_frame = false;
  bool painted = false;

  g::RunConfig cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 48;
  cfg.init = [&] { inits++; };
  cfg.frame = [&] {
    frames++;
    headless_in_frame = g::is_headless();
    g::begin_frame();
    afterhours::draw_rectangle(RectangleType{0, 0, 64, 48}, Color{0, 255, 0, 255});
    g::end_frame();
    if (frames == 3) {
      const auto capture =
          afterhours::capture_render_texture_rgba(g::get_render_texture());
      painted = capture && capture->pixels.size() >= 4 &&
                capture->pixels[1] > 249 && capture->pixels[0] < 6;
      g::request_quit();
    }
  };
  cfg.cleanup = [&] { cleanups++; };

  g::run(cfg);
  returned_from_run = true;

  check(inits == 1, "init runs once");
  check(frames == 3, "frames run until request_quit");
  check(cleanups == 1, "cleanup runs once");
  check(headless_in_frame, "frames run headless, with no window");
  check(painted, "drawing reaches the offscreen target");
  check(!g::is_headless(), "run shuts the headless device down");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
