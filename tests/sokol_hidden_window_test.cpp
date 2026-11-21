// R-137 sokol side: headless run has no window, so show/hide only move the
// recorded state — real visibility needs a human on a real window.
#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif
#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>
#include <cstdio>
namespace g = afterhours::graphics;
static int checks = 0, failures = 0;
static void check(bool c, const char *m) { ++checks; if (!c) { ++failures; std::fprintf(stderr, "FAIL: %s\n", m); } }
int main() {
  int frames = 0;
  g::RunConfig cfg;
  cfg.display = g::DisplayMode::Headless;
  cfg.width = 64; cfg.height = 48; cfg.start_hidden = true;
  cfg.frame = [&] {
    ++frames;
    check(!g::window_is_hidden(), "headless has no window to hide");
    g::hide_window();
    check(g::window_is_hidden(), "hide_window records hidden");
    g::show_window();
    check(!g::window_is_hidden(), "show_window records visible");
    g::request_quit();
  };
  g::run(cfg);
  check(frames == 1, "frame ran");
  check(g::metal_detail::g_start_hidden, "RunConfig start_hidden reached backend");
  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
