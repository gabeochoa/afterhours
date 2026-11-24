#pragma once

#include "core/system_timing.h"

#include <cstdint>

namespace afterhours::render_counters {

// Texture/RT creations happen outside the batched render pass, so backends
// stash them here and BatchedRenderer drains the stash into its per-frame
// stats. Empty when profiling is compiled out, so callers cost nothing.
#if AFTERHOURS_ENABLE_PROFILING
inline std::uint64_t &pending_creates() {
  static thread_local std::uint64_t count = 0;
  return count;
}

inline void note_create() { ++pending_creates(); }

inline std::uint64_t take_creates() {
  const auto count = pending_creates();
  pending_creates() = 0;
  return count;
}
#else
inline void note_create() {}

inline std::uint64_t take_creates() { return 0; }
#endif

} // namespace afterhours::render_counters
