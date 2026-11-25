#pragma once

#include <cstdint>
#include <span>

namespace afterhours {

inline constexpr std::uint64_t CORE_HASH_OFFSET_BASIS = 14695981039346656037ull;
inline constexpr std::uint64_t CORE_HASH_PRIME = 1099511628211ull;

[[nodiscard]] inline std::uint64_t core_hash(std::span<const std::uint8_t> bytes) {
  std::uint64_t h = CORE_HASH_OFFSET_BASIS;
  for (std::uint8_t b : bytes) { h ^= b; h *= CORE_HASH_PRIME; }
  return h;
}

} // namespace afterhours
