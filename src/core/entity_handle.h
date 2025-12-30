#pragma once

#include <cstdint>

namespace afterhours {

// Stable handle for identifying entities across create/delete churn.
// - `slot` indexes a stable slot table
// - `gen` is a generation counter to detect stale handles after deletion/reuse
//
// This is a simple POD type intended to be safe for caller-defined
// serialization (two fixed-width 32-bit integers).
struct EntityHandle {
  uint32_t slot;
  uint32_t gen;

  static constexpr uint32_t INVALID_SLOT = 0xFFFFFFFFu;

  static constexpr EntityHandle invalid() { return {INVALID_SLOT, 0}; }
  [[nodiscard]] constexpr bool valid() const { return slot != INVALID_SLOT; }

  friend constexpr bool operator==(const EntityHandle &a,
                                   const EntityHandle &b) {
    return a.slot == b.slot && a.gen == b.gen;
  }
  friend constexpr bool operator!=(const EntityHandle &a,
                                   const EntityHandle &b) {
    return !(a == b);
  }
};

} // namespace afterhours

