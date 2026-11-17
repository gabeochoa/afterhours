#include <afterhours/ah.h>

#include <cstdio>

using namespace afterhours;

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

static SystemManager counting_manager() {
  SystemManager sm;
  sm.register_fixed_update_system([](float dt) {
    ticks++;
    last_dt = dt;
  });
  return sm;
}

int main() {
  auto &entities = EntityHelper::get_entities_for_mod();

  {
    SystemManager sm = counting_manager();
    ticks = 0;
    sm.fixed_tick_all(entities, 0.1f);
    CHECK(ticks == 12);
    CHECK(last_dt == SystemManager::FIXED_TICK_RATE);
  }

  {
    SystemManager sm = counting_manager();
    sm.fixed_tick_rate = 1.f / 60.f;
    ticks = 0;
    sm.fixed_tick_all(entities, 0.1f);
    CHECK(ticks == 6);
    CHECK(last_dt == 1.f / 60.f);
  }

  {
    SystemManager sm = counting_manager();
    sm.fixed_tick_rate = 1.f / 60.f;
    sm.max_fixed_ticks = 4;
    ticks = 0;
    sm.fixed_tick_all(entities, 2.f);
    CHECK(ticks == 4);
    CHECK(sm.accumulator < sm.fixed_tick_rate);
    ticks = 0;
    sm.fixed_tick_all(entities, 1.f / 60.f);
    CHECK(ticks == 1);
  }

  printf("%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
