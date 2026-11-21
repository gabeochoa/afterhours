#include <afterhours/ah.h>

#include <cstdint>
#include <cstdio>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *expr, int line) {
  tests_run++;
  if (cond) tests_passed++;
  else fprintf(stderr, "  FAIL: %s  (line %d)\n", expr, line);
}
#define CHECK(expr) check((expr), #expr, __LINE__)

static uint32_t run_world(int ticks, int *render_calls) {
  uint32_t x = 12345u;
  SimulationDriver sim;
  sim.set_fixed_step(1.f / 60.f);
  sim.systems.register_fixed_update_system([&](float dt) {
    CHECK(dt == 1.f / 60.f);
    x = x * 1664525u + 1013904223u;
  });
  sim.systems.register_render_system([&](float) { (*render_calls)++; });
  CHECK(sim.step(ticks) == 0);  // not begun: no ticks, not finished
  CHECK(!sim.finished());
  sim.begin();
  CHECK(sim.step(ticks) == ticks);
  CHECK(sim.tick_count() == static_cast<uint64_t>(ticks));
  return x;
}

int main() {
  int renders_a = 0, renders_b = 0;
  const uint32_t a = run_world(120, &renders_a);
  const uint32_t b = run_world(120, &renders_b);
  CHECK(a == b);
  CHECK(renders_a == 0 && renders_b == 0);

  SimulationDriver sim;
  sim.set_fixed_step(0.1f);
  int ticks = 0;
  sim.systems.register_update_system([&](float dt) {
    CHECK(dt == 0.1f);
    ticks++;
  });
  sim.begin();
  sim.pause();
  CHECK(sim.step(3) == 0 && ticks == 0);
  sim.resume();
  CHECK(sim.advance(0.25f) == 2);
  sim.speed = 2.f;
  CHECK(sim.advance(0.25f) == 5);  // 0.05 carry + 0.5 at 2x = 5 whole 0.1 ticks
  CHECK(sim.tick_count() == 7);
  CHECK(sim.elapsed() == 7 * 0.1f);
  CHECK(sim.run_until([&] { return ticks >= 10; }, 2) == false);
  CHECK(sim.tick_count() == 9 && !sim.finished());
  CHECK(sim.run_until([&] { return ticks >= 10; }, 5) == true);
  CHECK(sim.finished() && sim.step(1) == 0);
  sim.reset();
  CHECK(sim.tick_count() == 0 && sim.elapsed() == 0.f && !sim.finished());

  printf("%d/%d checks passed\n", tests_passed, tests_run);
  return tests_passed == tests_run ? 0 : 1;
}
