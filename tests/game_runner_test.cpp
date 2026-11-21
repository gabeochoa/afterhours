// game_runner_test.cpp
// A minimal game runs N headless frames through the one game::run call:
// setup once, fixed-step updates, one draw per frame, cleanup once.
// Sokol headless is the backend that can draw with no window (the raylib
// RunConfig headless path creates no window at all).

#if !defined(AFTER_HOURS_USE_RAYLIB) && !defined(AFTER_HOURS_USE_VULKAN) && \
    !defined(AFTER_HOURS_USE_SOKOL)
#define AFTER_HOURS_USE_SOKOL
#endif

#include <afterhours/src/backends/sokol/drawing_helpers.h>
#include <afterhours/src/game.h>

#include <cstdio>
#include <string>

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
  printf("=== game::run one-call runner ===\n\n");

  int setups = 0, updates = 0, draws = 0, cleanups = 0;
  float moved = 0.f;

  afterhours::game::Config cfg;
  cfg.run.title = "game_runner_test";
  cfg.run.display = afterhours::graphics::DisplayMode::Headless;
  cfg.run.width = 320;
  cfg.run.height = 180;
  cfg.run.fixed_dt = 1.f / 60.f;
  cfg.run.sim_steps = 2;
  cfg.max_frames = 5;
  cfg.setup = [&] { setups++; };
  cfg.update = [&](float dt) {
    updates++;
    moved += 60.f * dt; // 60px/s at the fixed step is 1px per update
  };
  cfg.draw = [&] {
    draws++;
    afterhours::draw_rectangle(RectangleType{moved, 10.f, 8.f, 8.f},
                               afterhours::Color{255, 120, 0, 255});
  };
  cfg.cleanup = [&] { cleanups++; };

  const int rc = afterhours::game::run(cfg);

  check(rc == 0, "run returns a main()-style 0");
  check(setups == 1, "setup ran exactly once");
  check(draws == 5, "draw ran once per frame for max_frames frames");
  check(updates == 10, "update ran sim_steps times per frame at fixed_dt");
  check(moved > 9.9f && moved < 10.1f, "fixed steps moved the ball 10px");
  check(cleanups == 1, "cleanup ran exactly once");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
