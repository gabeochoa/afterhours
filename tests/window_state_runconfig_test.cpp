// window_state_runconfig_test.cpp
// Persisted window state must reach the window at creation time, not after
// a flash at the defaults. Size was already on RunConfig and already applied
// at creation by both backends; position was absent entirely. These check
// the plumbing values the backends read in run().

#define AFTER_HOURS_USE_RAYLIB

#include <afterhours/src/graphics_common.h>

#include <cstdio>
#include <string>

using afterhours::graphics::RunConfig;

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
  printf("=== RunConfig window state plumbing ===\n\n");

  {
    const RunConfig cfg;
    check(cfg.width == 1280 && cfg.height == 720,
          "size defaults are unchanged");
    check(cfg.window_x == -1 && cfg.window_y == -1,
          "position defaults to unset (backend placement)");
  }

  {
    RunConfig cfg;
    cfg.width = 1920;
    cfg.height = 1080;
    cfg.window_x = 120;
    cfg.window_y = 80;
    check(cfg.width == 1920 && cfg.height == 1080,
          "restored size is carried on the config");
    check(cfg.window_x == 120 && cfg.window_y == 80,
          "restored position is carried on the config");
    // The raylib backend applies position only when both are >= 0, and the
    // sokol backend warns when either is set. 0,0 is a real origin, not
    // "unset", so it must not collide with the -1 sentinel.
    cfg.window_x = 0;
    cfg.window_y = 0;
    check(cfg.window_x >= 0 && cfg.window_y >= 0,
          "origin 0,0 is a set position, distinct from unset");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
