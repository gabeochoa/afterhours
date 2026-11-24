#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>

#include <cmath>
#include <cstdio>
#include <string>

namespace g = afterhours::graphics;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== sokol mouse before first event ===\n\n");
  const auto pos = g::MetalPlatformAPI::get_mouse_position();
  printf("  pos=(%g, %g)\n", pos.x, pos.y);
  check(std::isfinite(pos.x) && std::isfinite(pos.y),
        "pre-init mouse position is finite");
  const auto delta = g::MetalPlatformAPI::get_mouse_delta();
  printf("  delta=(%g, %g)\n", delta.x, delta.y);
  check(std::isfinite(delta.x) && std::isfinite(delta.y),
        "pre-init mouse delta is finite");
  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  return checks_run == checks_passed ? 0 : 1;
}
