// Timer Plugin Performance Benchmarks
//
// Measures the cost of the timer plugin's update path over N entities:
//   - TimerUpdateSystem      (HasTimer::update via SystemManager)
//   - TriggerUpdateSystem    (TriggerOnDt::test via SystemManager)
//   - both systems together  (what a real frame pays)
//   - a raw update loop       (isolates the timer math from ECS dispatch)
//
// Sweeps N = 1k / 10k / 100k timer entities.
//
// To run (on a machine WITHOUT Santa Lockdown):  make && ./timer_benchmarks.exe
//
// Methodology:
//   - Compiled -O3 -DNDEBUG.
//   - Each frame calls sm.run(dt). We use auto_reset timers so the systems do
//     real work (fire + re-arm) every few frames rather than short-circuiting.
//   - The "raw" benchmarks call update()/test() directly over a std::vector to
//     show the floor cost of the arithmetic, so we can attribute any ECS
//     overhead correctly and judge whether micro-opts to update() can matter.

#include <memory>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/plugins/timer.h"

#define CATCH_CONFIG_ENABLE_BENCHMARKING
#define CATCH_CONFIG_MAIN
#include "../../../../vendor/catch2/catch.hpp"

using namespace afterhours;
using HasTimer = timer::HasTimer;
using TriggerOnDt = timer::TriggerOnDt;

static void cleanup_all() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// Populate N entities with a HasTimer (auto_reset so update() does full work).
static void make_timers(int n) {
  cleanup_all();
  for (int i = 0; i < n; ++i)
    EntityHelper::createEntity().addComponent<HasTimer>(0.5f, /*auto=*/true);
  EntityHelper::merge_entity_arrays();
}

static void make_triggers(int n) {
  cleanup_all();
  for (int i = 0; i < n; ++i)
    EntityHelper::createEntity().addComponent<TriggerOnDt>(0.5f);
  EntityHelper::merge_entity_arrays();
}

// ---------------------------------------------------------------------------
// TimerUpdateSystem (via SystemManager) — the real per-frame path.
// ---------------------------------------------------------------------------
TEST_CASE("timer_update_system", "[benchmark][timer]") {
  for (int n : {1000, 10000, 100000}) {
    make_timers(n);
    SystemManager sm;
    sm.register_update_system(std::make_unique<timer::TimerUpdateSystem>());
    BENCHMARK("TimerUpdateSystem tick, N=" + std::to_string(n)) {
      sm.run(0.16f);
      return EntityHelper::get_entities().size();
    };
  }
  cleanup_all();
}

// ---------------------------------------------------------------------------
// TriggerUpdateSystem (via SystemManager).
// ---------------------------------------------------------------------------
TEST_CASE("trigger_update_system", "[benchmark][timer]") {
  for (int n : {1000, 10000, 100000}) {
    make_triggers(n);
    SystemManager sm;
    sm.register_update_system(std::make_unique<timer::TriggerUpdateSystem>());
    BENCHMARK("TriggerUpdateSystem tick, N=" + std::to_string(n)) {
      sm.run(0.16f);
      return EntityHelper::get_entities().size();
    };
  }
  cleanup_all();
}

// ---------------------------------------------------------------------------
// Both systems together — what a frame with timers + triggers actually pays.
// This is the target for optimization #5 (combine into one pass).
// ---------------------------------------------------------------------------
TEST_CASE("both_systems", "[benchmark][timer]") {
  for (int n : {1000, 10000, 100000}) {
    cleanup_all();
    for (int i = 0; i < n; ++i) {
      auto &e = EntityHelper::createEntity();
      e.addComponent<HasTimer>(0.5f, true);
      e.addComponent<TriggerOnDt>(0.5f);
    }
    EntityHelper::merge_entity_arrays();
    SystemManager sm;
    sm.register_update_system(std::make_unique<timer::TimerUpdateSystem>());
    sm.register_update_system(std::make_unique<timer::TriggerUpdateSystem>());
    BENCHMARK("Timer+Trigger both systems tick, N=" + std::to_string(n)) {
      sm.run(0.16f);
      return EntityHelper::get_entities().size();
    };
  }
  cleanup_all();
}

// ---------------------------------------------------------------------------
// Raw update loop — floor cost of the arithmetic, no ECS/virtual dispatch.
// If the SystemManager numbers are much larger than these, the bottleneck is
// ECS iteration/dispatch, NOT the timer math — which means micro-opts to
// update() (branchless max, reciprocal, [[likely]], SIMD on the body) cannot
// move the real number.
// ---------------------------------------------------------------------------
TEST_CASE("raw_timer_update_loop", "[benchmark][timer][raw]") {
  for (int n : {1000, 10000, 100000}) {
    std::vector<HasTimer> timers;
    timers.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
      timers.emplace_back(0.5f, true);
    BENCHMARK("raw HasTimer::update loop, N=" + std::to_string(n)) {
      float acc = 0.f;
      for (auto &t : timers)
        acc += t.update(0.16f) ? 1.f : 0.f;
      return acc;
    };
  }
}

TEST_CASE("raw_trigger_test_loop", "[benchmark][timer][raw]") {
  for (int n : {1000, 10000, 100000}) {
    std::vector<TriggerOnDt> trigs;
    trigs.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
      trigs.emplace_back(0.5f);
    BENCHMARK("raw TriggerOnDt::test loop, N=" + std::to_string(n)) {
      float acc = 0.f;
      for (auto &t : trigs)
        acc += t.test(0.16f) ? 1.f : 0.f;
      return acc;
    };
  }
}
