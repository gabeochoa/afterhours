// animation_test.cpp
// Behavior lock for src/plugins/animation.h.
//
// The contract we must preserve across any perf refactor of the animation
// plugin is the ANIMATED VALUE TRAJECTORY: for a fixed animation spec, the
// per-frame `current` value, the frame on which a track goes inactive, the
// frame on which on_complete fires, queue/sequence advancement, and watcher
// (on_step / on_change) quantized-callback firing must all be byte-identical
// before and after optimization.
//
// These are golden-trajectory tests: we sample the track across N frames with
// a fixed dt and assert exact values (bit-exact where the math is exact,
// tight epsilon where float easing is involved). Any drift = regression.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <optional>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../src/plugins/animation.h"

using namespace afterhours;
using EZ = animation::EasingType;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);            \
    }                                                                          \
  } while (0)

// Exact float equality: the trajectory must not drift AT ALL. We use bit-exact
// comparison so that even a 1-ULP change (e.g. from reordering lerp math) is
// caught. If an intentional, documented behavior change is made, the golden
// values here are updated in the same commit.
static bool feq_exact(float a, float b) {
  return std::memcmp(&a, &b, sizeof(float)) == 0;
}

// Distinct enum keys per test so the process-global manager<>() singletons do
// not alias across tests.
enum struct K1 : size_t { A };
enum struct K2 : size_t { A };
enum struct K3 : size_t { A };
enum struct K4 : size_t { A };
enum struct K5 : size_t { A };
enum struct K6 : size_t { A };
enum struct K7 : size_t { A };

// ---------------------------------------------------------------------------
// 1. Linear single segment: exact trajectory + completion frame.
// ---------------------------------------------------------------------------
static void test_linear_trajectory() {
  std::printf("test_linear_trajectory\n");
  auto &mgr = animation::manager<K1>();
  animation::anim<K1>(K1::A).from(0.f).to(10.f, 1.0f, EZ::Linear);

  const float dt = 0.1f;
  // Golden: current after each 0.1s step. lerp(0,10,elapsed/1.0) with elapsed
  // accumulating by 0.1f each frame (float accumulation reproduced exactly).
  float elapsed = 0.f;
  for (int frame = 0; frame < 9; ++frame) {
    mgr.update(dt);
    elapsed += dt;
    float u = animation::apply_ease(EZ::Linear, elapsed / 1.0f);
    float expected = std::lerp(0.f, 10.f, u);
    auto v = mgr.get_value(K1::A);
    CHECK(v.has_value(), "linear active mid-flight");
    CHECK(v.has_value() && feq_exact(v.value(), expected),
          "linear value bit-exact per frame");
    CHECK(mgr.is_active(K1::A), "linear still active before completion");
  }
  // 10th frame: elapsed reaches ~1.0 -> completes, snaps to target, inactive.
  mgr.update(dt);
  CHECK(!mgr.is_active(K1::A), "linear inactive after completion frame");
  // get_value returns nullopt once inactive.
  CHECK(!mgr.get_value(K1::A).has_value(), "linear no value once inactive");
}

// ---------------------------------------------------------------------------
// 2. EaseOutQuad trajectory (non-linear) exactness.
// ---------------------------------------------------------------------------
static void test_easeout_trajectory() {
  std::printf("test_easeout_trajectory\n");
  auto &mgr = animation::manager<K2>();
  animation::anim<K2>(K2::A).from(0.f).to(100.f, 1.0f, EZ::EaseOutQuad);
  const float dt = 0.2f;
  float elapsed = 0.f;
  for (int frame = 0; frame < 4; ++frame) {
    mgr.update(dt);
    elapsed += dt;
    float u = animation::apply_ease(EZ::EaseOutQuad, elapsed / 1.0f);
    float expected = std::lerp(0.f, 100.f, u);
    auto v = mgr.get_value(K2::A);
    CHECK(v.has_value() && feq_exact(v.value(), expected),
          "easeout value bit-exact per frame");
  }
  // Frame 5 -> elapsed 1.0 -> complete.
  mgr.update(dt);
  CHECK(!mgr.is_active(K2::A), "easeout inactive after completion");
}

// ---------------------------------------------------------------------------
// 3. Sequence / queue advancement: value + which segment we are on per frame.
// ---------------------------------------------------------------------------
static void test_sequence_advancement() {
  std::printf("test_sequence_advancement\n");
  auto &mgr = animation::manager<K3>();
  animation::anim<K3>(K3::A).from(1.f).sequence({
      {.to_value = 2.f, .duration = 0.5f, .easing = EZ::Linear},
      {.to_value = 0.5f, .duration = 0.5f, .easing = EZ::Linear},
      {.to_value = 1.f, .duration = 0.5f, .easing = EZ::Linear},
  });

  // After first 0.5s segment -> exactly 2.0 (completion snaps to target).
  mgr.update(0.5f);
  auto v = mgr.get_value(K3::A);
  CHECK(v.has_value() && feq_exact(v.value(), 2.f), "seg1 end == 2.0");
  CHECK(mgr.is_active(K3::A), "still active into seg2");

  mgr.update(0.5f);
  v = mgr.get_value(K3::A);
  CHECK(v.has_value() && feq_exact(v.value(), 0.5f), "seg2 end == 0.5");

  mgr.update(0.5f);
  // seg3 completes on this frame -> snaps to 1.0 then goes inactive.
  CHECK(!mgr.is_active(K3::A), "inactive after seg3");
}

// ---------------------------------------------------------------------------
// 4. on_complete fires exactly once, on the completion frame, only when the
//    queue is empty.
// ---------------------------------------------------------------------------
static void test_on_complete_timing() {
  std::printf("test_on_complete_timing\n");
  auto &mgr = animation::manager<K4>();
  int fired = 0;
  animation::anim<K4>(K4::A)
      .from(0.f)
      .to(1.f, 0.3f, EZ::Linear)
      .on_complete([&fired]() { ++fired; });

  mgr.update(0.1f);
  CHECK(fired == 0, "on_complete not fired mid-flight (f1)");
  mgr.update(0.1f);
  CHECK(fired == 0, "on_complete not fired mid-flight (f2)");
  mgr.update(0.1f); // elapsed 0.3 -> complete
  CHECK(fired == 1, "on_complete fired exactly on completion frame");
  mgr.update(0.1f);
  CHECK(fired == 1, "on_complete not fired again after completion");
}

// ---------------------------------------------------------------------------
// 5. on_step watcher: quantized callback fires on the exact frames where the
//    quantized bucket changes, with the exact bucket values.
// ---------------------------------------------------------------------------
static void test_watcher_step() {
  std::printf("test_watcher_step\n");
  auto &mgr = animation::manager<K5>();
  std::vector<int> steps;
  // 0 -> 10 over 1s, step size 1.0. floor(current/1.0) buckets.
  animation::anim<K5>(K5::A)
      .from(0.f)
      .to(10.f, 1.0f, EZ::Linear)
      .on_step(1.0f, [&steps](int q) { steps.push_back(q); });

  const float dt = 0.1f; // each frame current advances by ~1.0
  for (int frame = 0; frame < 10; ++frame)
    mgr.update(dt);

  // Reconstruct the golden bucket sequence by replaying the exact same float
  // math the plugin uses (independent of implementation internals).
  std::vector<int> golden;
  {
    float elapsed = 0.f;
    std::optional<int> last;
    for (int frame = 0; frame < 10; ++frame) {
      elapsed += dt;
      float u = animation::apply_ease(EZ::Linear, elapsed / 1.0f);
      // Note: watcher is notified with new_current BEFORE the completion snap.
      float new_current = std::lerp(0.f, 10.f, u);
      int q = static_cast<int>(std::floor(new_current / 1.0f));
      if (!last.has_value() || q != *last) {
        last = q;
        golden.push_back(q);
      }
      if (elapsed >= 1.0f)
        break; // track goes inactive; no further watcher notifications
    }
  }
  CHECK(steps == golden, "watcher step bucket sequence identical to golden");
  std::printf("    (watcher fired %zu times, golden %zu)\n", steps.size(),
              golden.size());
}

// ---------------------------------------------------------------------------
// 6. is_active / get_value semantics for an untouched key.
// ---------------------------------------------------------------------------
static void test_untouched_key() {
  std::printf("test_untouched_key\n");
  auto &mgr = animation::manager<K6>();
  CHECK(!mgr.is_active(K6::A), "untouched key inactive");
  CHECK(!mgr.get_value(K6::A).has_value(), "untouched key no value");
}

// ---------------------------------------------------------------------------
// 7. Zero-duration segment: snaps to target and completes on first update.
// ---------------------------------------------------------------------------
static void test_zero_duration() {
  std::printf("test_zero_duration\n");
  auto &mgr = animation::manager<K7>();
  int fired = 0;
  animation::anim<K7>(K7::A)
      .from(0.f)
      .to(5.f, 0.0f, EZ::Linear)
      .on_complete([&fired]() { ++fired; });
  mgr.update(0.016f);
  // Per current code: duration<=0 branch snaps current=to, active=false, but
  // does NOT run the queue/on_complete path. Lock that exact behavior.
  CHECK(!mgr.is_active(K7::A), "zero-duration inactive after first update");
  auto v = mgr.get_value(K7::A);
  CHECK(!v.has_value(), "zero-duration inactive -> get_value nullopt");
  CHECK(fired == 0, "zero-duration does not fire on_complete (current behavior)");
}

int main() {
  std::printf("=== animation.h behavior lock ===\n");
  test_linear_trajectory();
  test_easeout_trajectory();
  test_sequence_advancement();
  test_on_complete_timing();
  test_watcher_step();
  test_untouched_key();
  test_zero_duration();
  std::printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
