// animation_bench.cpp
//
// Micro-benchmark for src/plugins/animation.h `AnimationManager::update(dt)`,
// the per-frame hot path. Compiled -O3 -DNDEBUG.
//
//   clang++ -std=c++23 -O3 -DNDEBUG -I../../../vendor animation_bench.cpp \
//       -o animation_bench.exe && ./animation_bench.exe
//
// What it measures, and WHY:
//   update() iterates the ENTIRE track map every frame:
//       for (auto &kv : tracks) { if (!tr.active) continue; ... }
//   So its cost scales with TOTAL tracks (map size), not just ACTIVE tracks.
//   The interesting regime for the plugin doc's item #1/#2 is "many tracks
//   exist, few are active" -- e.g. a UI with thousands of registered
//   animation keys where only a handful animate on a given frame. We sweep:
//     - total N tracks in the map
//     - fraction active
//   and measure the cost of one update() sweep. A contiguous active-track
//   list (item #1) would make the ALL-INACTIVE and FEW-ACTIVE columns flat in
//   N; the current map makes them grow linearly in N (map traversal + the
//   `if (!tr.active) continue` visit per node, plus pointer-chasing across
//   heap-scattered unordered_map nodes -> cache misses).
//
// Santa note: on the target machine freshly-compiled binaries are SIGKILLed
// (exit 137), so this did not run here. The code is runnable on an unlocked
// box. See docs/speed/animation.md for the analytical argument used instead.

#include <chrono>
#include <cstdio>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../../src/plugins/animation.h"

using namespace afterhours;
using EZ = animation::EasingType;
using Clock = std::chrono::steady_clock;

// We build a LOCAL AnimationManager<CompositeKey> per cell (it is a public
// nested type) so each (N, active%) measurement is perfectly isolated -- the
// map contains exactly this cell's `total` tracks and nothing else. We drive it
// through the same public API the plugin uses (ensure_track + update), so we
// measure the real per-frame code path, not a reimplementation.
static double bench_update(size_t total, double active_frac, int frames) {
  animation::AnimationManager<animation::CompositeKey> mgr;
  size_t active_count = static_cast<size_t>(total * active_frac);
  for (size_t i = 0; i < total; ++i) {
    auto &tr = mgr.ensure_track(animation::CompositeKey{0, i});
    tr.from = 0.f;
    tr.current = 0.f;
    if (i < active_count) {
      tr.to = 1.f;
      tr.duration = 1e6f; // stays active across all measured frames
      tr.current_easing = EZ::Linear;
      tr.elapsed = 0.f;
      tr.active = true;
    }
    // inactive tracks: active stays false (the `if (!tr.active) continue` case)
  }

  // Warm one pass (touch pages), then time `frames` sweeps.
  mgr.update(0.f);
  auto t0 = Clock::now();
  for (int f = 0; f < frames; ++f)
    mgr.update(1.f / 60.f);
  auto t1 = Clock::now();
  double ns =
      std::chrono::duration<double, std::nano>(t1 - t0).count() / frames;
  return ns; // ns per update() sweep
}

int main() {
  std::printf("=== animation.h update() sweep bench (ns per update) ===\n");
  std::printf("%-10s %-12s %-12s %-12s\n", "N tracks", "0%% active",
              "1%% active", "100%% active");
  const int frames = 2000;
  for (size_t N : {size_t(64), size_t(256), size_t(1024), size_t(4096),
                   size_t(16384)}) {
    double a0 = bench_update(N, 0.0, frames);
    double a1 = bench_update(N, 0.01, frames);
    double a100 = bench_update(N, 1.0, frames);
    std::printf("%-10zu %-12.1f %-12.1f %-12.1f\n", N, a0, a1, a100);
  }
  std::printf("\nInterpretation: with the current unordered_map, the 0%%- and\n"
              "1%%-active columns should grow ~linearly with N (whole map is\n"
              "walked every frame). A contiguous active-track list keeps those\n"
              "columns flat. The 100%%-active column is work that must happen\n"
              "regardless and is the fair floor.\n");
  return 0;
}
