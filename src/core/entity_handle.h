#pragma once

#include <cstddef>
#include <limits>

namespace afterhours {

// Stable handle for identifying entities across create/delete churn.
// - `slot` indexes a stable slot table
// - `gen` is a generation counter to detect stale handles after deletion/reuse
//
// This is a POD type intended to be serialization-friendly (two 32-bit ints).
struct EntityHandle {
  using Slot = std::size_t;

  Slot slot;
  std::size_t gen;

  static constexpr Slot INVALID_SLOT = (std::numeric_limits<Slot>::max)();

  static constexpr EntityHandle invalid() { return {INVALID_SLOT, 0}; }

  [[nodiscard]] constexpr bool valid() const { return slot != INVALID_SLOT; }
  [[nodiscard]] constexpr bool is_valid() const { return valid(); }
  [[nodiscard]] constexpr bool is_invalid() const { return !valid(); }
};

} // namespace afterhours

