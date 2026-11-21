// --time-scale on a fixed-step sim: more fixed steps, never a bigger dt.

#include <afterhours/ah.h>

#include <afterhours/src/plugins/e2e_testing/harness.h>

#include <cstdio>

using namespace afterhours;
using namespace afterhours::testing;

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *expr, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static int ticks = 0;
static float last_dt = 0.f;

int main() {
  CHECK(substeps_for_time_scale(4.f) == 4);
  CHECK(substeps_for_time_scale(1.f) == 1);
  CHECK(substeps_for_time_scale(0.5f) == 1);

  auto &entities = EntityHelper::get_entities_for_mod();

  // Scale 4: four counted ticks, each of the fixed dt, not one of 4x dt.
  {
    graphics::RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    CHECK(apply_time_scale(cfg, 4.f) == 4);
    CHECK(cfg.step_count() == 4);

    SystemManager sm;
    sm.register_fixed_update_system([](float dt) {
      ticks++;
      last_dt = dt;
    });
    ticks = 0;
    run_fixed_steps(sm, entities, cfg, 0.100f);
    CHECK(ticks == 4);
    CHECK(last_dt == cfg.fixed_dt);
  }

  // Scale 1 is unchanged: one tick of the fixed dt.
  {
    graphics::RunConfig cfg;
    cfg.fixed_dt = 1.f / 60.f;
    CHECK(apply_time_scale(cfg, 1.f) == 1);

    SystemManager sm;
    sm.register_fixed_update_system([](float dt) {
      ticks++;
      last_dt = dt;
    });
    ticks = 0;
    run_fixed_steps(sm, entities, cfg, 0.100f);
    CHECK(ticks == 1);
    CHECK(last_dt == cfg.fixed_dt);
  }

  // No fixed_dt: apply is a no-op, wall-clock dt stays the (scaled) mode.
  {
    graphics::RunConfig cfg;
    CHECK(apply_time_scale(cfg, 4.f) == 1);
    CHECK(cfg.step_count() == 1);
  }

  printf("%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
