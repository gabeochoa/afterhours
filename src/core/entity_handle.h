#pragma once

#include <cstdint>

namespace afterhours {

// Stable handle for identifying entities across create/delete churn.
// - `slot` indexes a stable slot table
// - `gen` is a generation counter to detect stale handles after deletion/reuse
//
// This is a POD type intended to be serialization-friendly (two 32-bit ints).
struct EntityHandle {
  std::uint32_t slot;
  std::uint32_t gen;

  static constexpr std::uint32_t INVALID_SLOT = 0xFFFFFFFFu;

  static constexpr EntityHandle invalid() { return {INVALID_SLOT, 0u}; }

  [[nodiscard]] constexpr bool valid() const { return slot != INVALID_SLOT; }
  [[nodiscard]] constexpr bool is_valid() const { return valid(); }
  [[nodiscard]] constexpr bool is_invalid() const { return !valid(); }
};

} // namespace afterhours

