#pragma once

#include <cstdint>
#include <limits>

namespace afterhours {

// Stable handle for identifying entities across create/delete churn.
// - `slot` indexes a stable slot table
// - `gen` is a generation counter to detect stale handles after deletion/reuse
//
struct EntityHandle {
  // Fixed-size types for entity handles to ensure consistent serialization
  // and avoid platform-dependent size_t variations
  using Slot = std::uint32_t;
  using Generation = std::uint32_t;

  Slot slot;
  Generation gen;

  static constexpr Slot INVALID_SLOT = (std::numeric_limits<Slot>::max)();

  static constexpr EntityHandle invalid() { return {INVALID_SLOT, 0}; }

  [[nodiscard]] constexpr bool valid() const { return slot != INVALID_SLOT; }
  [[nodiscard]] constexpr bool is_valid() const { return valid(); }
  [[nodiscard]] constexpr bool is_invalid() const { return !valid(); }
};

} // namespace afterhours
