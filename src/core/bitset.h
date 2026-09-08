#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace afterhours {

// std::bitset's interface without its unoptimised cost.
//
// We build at -O0 and puzzle builds at -Og. In both, libc++'s
// bitset::operator[] const returns a __const_reference proxy, so reading one
// bit is a handful of un-inlined calls, two through DYLD stubs. Entity's
// has_child_of does that 128 times per entity per system, which puzzle sampled
// at 37% of a frame.
//
// test() is not a way out -- libc++'s adds a bounds check and then calls
// operator[] anyway -- and the conforming bool return is behind an unstable-ABI
// flag. So: same names, same meanings, plain words underneath. Identical to
// std::bitset at -O2, one instruction at -O0.
template <std::size_t N> struct Bitset {
  static constexpr std::size_t bits_per_word = 64;
  static constexpr std::size_t num_words =
      (N + bits_per_word - 1) / bits_per_word;

  std::uint64_t words[num_words] = {};

  [[nodiscard]] constexpr bool test(std::size_t pos) const {
    return (words[pos / bits_per_word] >> (pos % bits_per_word)) & 1ull;
  }

  [[nodiscard]] constexpr bool operator[](std::size_t pos) const {
    return test(pos);
  }

  constexpr void set(std::size_t pos, bool value = true) {
    const std::uint64_t bit = 1ull << (pos % bits_per_word);
    if (value) {
      words[pos / bits_per_word] |= bit;
    } else {
      words[pos / bits_per_word] &= ~bit;
    }
  }

  constexpr void reset(std::size_t pos) { set(pos, false); }

  constexpr void reset() {
    for (std::size_t w = 0; w < num_words; w++) {
      words[w] = 0;
    }
  }

  [[nodiscard]] constexpr bool any() const {
    for (std::size_t w = 0; w < num_words; w++) {
      if (words[w])
        return true;
    }
    return false;
  }

  [[nodiscard]] constexpr bool none() const { return !any(); }

  [[nodiscard]] constexpr std::size_t count() const {
    std::size_t total = 0;
    for (std::size_t w = 0; w < num_words; w++) {
      for (std::uint64_t bits = words[w]; bits; bits &= bits - 1) {
        total++;
      }
    }
    return total;
  }

  [[nodiscard]] constexpr std::size_t size() const { return N; }

  // Whole words, so a scan skips 64 empty slots at a time instead of one.
  // has_child_of is the caller that cares.
  [[nodiscard]] constexpr std::uint64_t word(std::size_t w) const {
    return words[w];
  }

  // Only for the AFTER_HOURS_DEBUG traces, which print the set.
  [[nodiscard]] std::string to_string() const {
    std::string out;
    out.reserve(N);
    for (std::size_t i = N; i-- > 0;) {
      out.push_back(test(i) ? '1' : '0');
    }
    return out;
  }
};

} // namespace afterhours
