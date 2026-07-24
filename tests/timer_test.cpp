// timer_test.cpp
// Behavior/correctness tests for the timer plugin (src/plugins/timer.h).
//
// Captures the CURRENT semantics of HasTimer, TriggerOnDt, and the two update
// systems so any perf refactor can be checked against a regression baseline.
//
// Build (from tests/):   make timer_test && ./timer_test
// Or standalone:
//   clang++ -std=c++20 -isystem .. tests/timer_test.cpp -o /tmp/tt && /tmp/tt

#include <cstdio>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
// Relative includes (not <afterhours/...>) so this always resolves against THIS
// worktree, regardless of what sits beside it on -isystem.
#include "../ah.h"
#include "../src/plugins/timer.h"

using namespace afterhours;
using HasTimer = timer::HasTimer;
using TriggerOnDt = timer::TriggerOnDt;

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static bool approx(float a, float b, float eps = 1e-5f) {
  float d = a - b;
  return (d < 0 ? -d : d) <= eps;
}

// ---- HasTimer unit semantics -------------------------------------------

static void test_fires_at_reset_time() {
  HasTimer t(1.0f, /*auto_reset=*/false);
  CHECK(!t.update(0.5f)); // 0.5 < 1.0
  CHECK(!t.is_ready());
  CHECK(t.update(0.5f)); // 1.0 >= 1.0 -> fires
  CHECK(t.is_ready());
}

static void test_non_auto_reset_stays_ready() {
  HasTimer t(1.0f, false);
  CHECK(t.update(1.0f)); // fires
  // Not auto-reset: stays expired and keeps firing on subsequent updates.
  CHECK(t.is_ready());
  CHECK(t.update(0.1f)); // still ready -> still returns true
  CHECK(approx(t.current_time, 1.1f));
}

static void test_auto_reset_rearms() {
  HasTimer t(1.0f, /*auto_reset=*/true);
  CHECK(t.update(1.0f)); // fires and resets
  CHECK(approx(t.current_time, 0.0f));
  CHECK(!t.is_ready());
  CHECK(!t.update(0.5f));
  CHECK(t.update(0.5f)); // fires again
}

static void test_paused_does_not_advance() {
  HasTimer t(1.0f, false);
  t.pause();
  CHECK(t.paused);
  CHECK(!t.update(5.0f)); // paused: no advance, no fire
  CHECK(approx(t.current_time, 0.0f));
  CHECK(!t.is_ready());
  t.add_time(5.0f); // add_time also respects pause
  CHECK(approx(t.current_time, 0.0f));
  t.resume();
  CHECK(t.update(1.0f)); // now it advances and fires
}

static void test_get_progress() {
  HasTimer t(4.0f, false);
  t.update(1.0f);
  CHECK(approx(t.get_progress(), 0.25f));
  t.update(1.0f);
  CHECK(approx(t.get_progress(), 0.5f));
  // reset_time <= 0 -> progress is 1.0 (guard against div-by-zero)
  HasTimer z(0.0f, false);
  CHECK(approx(z.get_progress(), 1.0f));
}

static void test_get_remaining() {
  HasTimer t(2.0f, false);
  t.update(0.5f);
  CHECK(approx(t.get_remaining(), 1.5f));
  t.update(2.0f); // overshoot
  // remaining clamps at 0, never negative
  CHECK(approx(t.get_remaining(), 0.0f));
}

static void test_set_and_reset() {
  HasTimer t(2.0f, false);
  t.set_time(1.5f);
  CHECK(approx(t.current_time, 1.5f));
  t.reset();
  CHECK(approx(t.current_time, 0.0f));
}

// ---- TriggerOnDt one-shot semantics ------------------------------------

static void test_trigger_one_shot() {
  TriggerOnDt tr(1.0f);
  CHECK(!tr.test(0.5f));
  CHECK(tr.test(0.5f));            // reaches 1.0 -> fires and resets current
  CHECK(approx(tr.current, 0.0f)); // auto-resets to 0
  CHECK(!tr.test(0.5f));           // cycle restarts
  CHECK(tr.test(0.5f));            // fires again
}

// ---- System-level: driving via SystemManager over real entities --------

static void test_timer_update_system_drives_entities() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  const int N = 100;
  for (int i = 0; i < N; ++i)
    EntityHelper::createEntity().addComponent<HasTimer>(1.0f, /*auto=*/true);
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  timer::register_update_systems(sm);

  // One tick of dt=1.0 should fire (and, being auto_reset, re-arm) every timer.
  sm.run(1.0f);

  int ready_or_rearmed = 0;
  for (auto &e : EntityHelper::get_entities()) {
    if (!e) continue;
    if (e->has<HasTimer>()) {
      auto &t = e->get<HasTimer>();
      // auto_reset fired => current_time back to 0
      if (approx(t.current_time, 0.0f)) ready_or_rearmed++;
    }
  }
  CHECK(ready_or_rearmed == N);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

static void test_trigger_update_system_drives_entities() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  const int N = 50;
  for (int i = 0; i < N; ++i)
    EntityHelper::createEntity().addComponent<TriggerOnDt>(2.0f);
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  timer::register_update_systems(sm);

  sm.run(1.0f); // current = 1.0, not yet fired
  int mid = 0;
  for (auto &e : EntityHelper::get_entities()) {
    if (e && e->has<TriggerOnDt>() && approx(e->get<TriggerOnDt>().current, 1.0f))
      mid++;
  }
  CHECK(mid == N);

  sm.run(1.0f); // current reaches 2.0 -> fires and resets to 0
  int fired = 0;
  for (auto &e : EntityHelper::get_entities()) {
    if (e && e->has<TriggerOnDt>() && approx(e->get<TriggerOnDt>().current, 0.0f))
      fired++;
  }
  CHECK(fired == N);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

int main() {
  printf("=== timer plugin behavior tests ===\n\n");
  test_fires_at_reset_time();
  test_non_auto_reset_stays_ready();
  test_auto_reset_rearms();
  test_paused_does_not_advance();
  test_get_progress();
  test_get_remaining();
  test_set_and_reset();
  test_trigger_one_shot();
  test_timer_update_system_drives_entities();
  test_trigger_update_system_drives_entities();

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
