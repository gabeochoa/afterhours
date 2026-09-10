// fixed_timestep_test.cpp
// Wall-clock dt is not reproducible: endless-dance-chaos ran one script twice
// and got 64 agents then 76. --time-scale made it worse by multiplying dt, so
// speed has to come from more steps, never a bigger one.

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
  printf("=== fixed timestep ===\n\n");

  // Unset: wall clock, as before.
  {
    RunConfig cfg;
    printf("  default:        %d step(s) of %.4fs\n", cfg.step_count(),
           cfg.step_dt(0.020f));
    check(cfg.step_count() == 1 && cfg.step_dt(0.020f) == 0.020f,
          "with no fixed_dt it is one step of the wall clock");
  }

  {
    RunConfig cfg;
    check(cfg.step_count() == 1 && cfg.step_dt(0.010f, 4.f) == 0.040f,
          "time_scale still stretches a wall-clock frame");
  }

  // Fixed: the step is the step, whatever the frame took.
  {
    RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    const float slow = cfg.step_dt(0.100f);
    const float fast = cfg.step_dt(0.001f);
    printf("  fixed, 100ms:   %.5fs\n", slow);
    printf("  fixed, 1ms:     %.5fs\n", fast);
    check(slow == fast, "a slow frame and a fast one step by the same amount");
    check(slow == cfg.fixed_dt, "and that amount is the one asked for");
  }

  // Speed comes from more steps, not a longer one.
  {
    RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    cfg.sim_steps = 12;
    printf("  fixed, 12 steps: %d step(s) of %.5fs\n", cfg.step_count(),
           cfg.step_dt(0.016f));
    check(cfg.step_count() == 12, "sim_steps is how many times to step");
    check(cfg.step_dt(0.016f) == cfg.fixed_dt,
          "and each one is still fixed_dt, not 12x it");
  }

  // time_scale must not touch a fixed step: silent, different numbers.
  {
    RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    const float plain = cfg.step_dt(0.016f, 1.f);
    const float scaled = cfg.step_dt(0.016f, 60.f);
    printf("  time_scale 60:  %.5fs\n", scaled);
    check(scaled == plain,
          "time_scale does not stretch a fixed step, it would change behaviour");
  }

  {
    RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    cfg.sim_steps = 0;
    check(cfg.step_count() >= 1,
          "sim_steps of 0 still steps once rather than freezing");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
