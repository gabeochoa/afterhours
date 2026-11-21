#pragma once

// Bit-stable RNG (G-P12). The streams here produce the same bits on every
// standard library and compiler:
//
//   - The engine is std::mt19937. Unlike std::hash and the uniform
//     distributions, its algorithm and seeding are fully specified by the
//     C++ standard (Mersenne Twister MT19937), so its raw 32-bit output
//     sequence for a given seed is identical on libc++, libstdc++ and MSVC.
//     Swapping it for a bespoke generator would gain nothing and would
//     break engine() / the save format, so it stays.
//   - String seeds hash with FNV-1a 32-bit (below). std::hash is
//     implementation-defined and disagreed across STLs.
//   - Integer ranges map via the specified Lemire multiply-shift scheme
//     (see get_int); floats via a fixed 24-bit construction (see get_float).
//     std::uniform_int_distribution / std::uniform_real_distribution are
//     implementation-defined and are no longer used here.
//   - next_u32() is the primitive draw: every mapping is a pure function
//     of raw draws, so pinning the literals in random_engine_test pins the
//     whole stream.
//
// CONSEQUENCE: values differ from previous afterhours releases. Persisted
// RNG state (save() text) still loads, but the draws after restore are the
// new sequence, not the old one. Anyone with persisted RNG state or golden
// draws must re-baseline. (See CHANGELOG.)
//
// The default constructor seeds from std::random_device and is therefore
// nondeterministic by choice; pass a seed for a reproducible stream.

#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>

#include "logging.h"
#include "singleton.h"

namespace afterhours {

struct RngStream {
  RngStream() : RngStream(std::random_device{}()) {}
  explicit RngStream(std::uint32_t new_seed) { set_seed(new_seed); }
  explicit RngStream(const std::string &new_seed) { set_seed(new_seed); }

  // Specified string-seed hash: FNV-1a 32-bit over the bytes.
  //   h = 2166136261; for each byte b: h ^= b; h *= 16777619 (mod 2^32).
  // Same string -> same seed on every platform; get_seed() returns h.
  void set_seed(const std::string &new_seed) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char byte : new_seed) {
      hash ^= byte;
      hash *= 16777619u;
    }
    set_seed(hash);
  }
  void set_seed(std::uint32_t new_seed) {
    seed = new_seed;
    rng.seed(new_seed);
  }
  [[nodiscard]] std::uint32_t get_seed() const { return seed; }

  // One raw 32-bit draw. The mt19937 output sequence is standard-specified,
  // so this is bit-identical across STLs/compilers for a given seed state.
  [[nodiscard]] std::uint32_t next_u32() { return rng(); }

  // Inclusive range [a, b]. Specified mapping (Lemire, 32-bit): each draw r
  // maps to (uint64)r * n >> 32, with rare rejection to stay unbiased.
  // With n == 1 and the full int range there is no rejection probability
  // beyond the fix-up below; every non-rejected call consumes exactly one
  // raw draw. a > b is a caller bug (the old std::uniform_* backend had UB
  // there) -> fail loud: log_error aborts in projects that wire it to
  // assert, surfacing the bad value immediately instead of papering over
  // it. a == b is a valid single-value range.
  [[nodiscard]] int get_int(int a, int b) {
    if (a > b)
      log_error("RngStream::get_int: inverted range [{}, {}] (a must be <= b)",
                a, b);
    const std::int64_t span = static_cast<std::int64_t>(b) -
                              static_cast<std::int64_t>(a) + 1; // [1, 2^32]
    std::int64_t offset = 0;
    if (span == (std::int64_t{1} << 32)) {
      // Full int range: a raw draw is already a complete, unbiased sample.
      offset = static_cast<std::int64_t>(next_u32());
    } else {
      const std::uint32_t n = static_cast<std::uint32_t>(span);
      std::uint32_t draw = next_u32();
      std::uint64_t mixed = static_cast<std::uint64_t>(draw) * n;
      std::uint32_t low = static_cast<std::uint32_t>(mixed);
      if (low < n) {
        // Reject the biased prefix; threshold is (-n) mod n in 32-bit
        // arithmetic, i.e. (2^32 - n) mod n.
        const std::uint32_t threshold =
            static_cast<std::uint32_t>(0u - n) % n;
        while (low < threshold) {
          draw = next_u32();
          mixed = static_cast<std::uint64_t>(draw) * n;
          low = static_cast<std::uint32_t>(mixed);
        }
      }
      offset = static_cast<std::int64_t>(
          static_cast<std::uint32_t>(mixed >> 32));
    }
    return static_cast<int>(static_cast<std::int64_t>(a) + offset);
  }
  // Range [a, b). Specified mapping: take the top 24 bits of one raw draw,
  // scale by 2^-24 into [0, 1) exactly, then interpolate a + u*(b-a) in
  // double via std::fma (exactly rounded per the standard), so the float
  // result is bit-identical across compilers — no FMA-contraction drift
  // between a * b + c and fma(a, b, c). a > b is a caller bug (UB in the
  // old backend) -> fail loud; a == b is a valid empty range yielding a.
  [[nodiscard]] float get_float(float a, float b) {
    if (a > b)
      log_error(
          "RngStream::get_float: inverted range [{}, {}) (a must be <= b)", a,
          b);
    const double unit =
        static_cast<double>(next_u32() >> 8) * (1.0 / 16777216.0); // 2^-24
    return static_cast<float>(std::fma(unit,
                                       static_cast<double>(b) -
                                           static_cast<double>(a),
                                       static_cast<double>(a)));
  }
  [[nodiscard]] bool get_bool() { return get_int(0, 1) == 1; }

  // Random valid index into a sized container; -1 when empty.
  template <typename T> [[nodiscard]] int get_index(const T &container) {
    if (container.size() == 0)
      return -1;
    return get_int(0, static_cast<int>(container.size()) - 1);
  }

  [[nodiscard]] std::size_t operator()(std::size_t count) {
    return count == 0 ? 0
                      : static_cast<std::size_t>(
                            get_int(0, static_cast<int>(count) - 1));
  }

  // Format is the mt19937 stream format (seed + full engine state). It is
  // unchanged since this serialized form was introduced, so old saves load;
  // what is new is the specified meaning of every subsequent draw.
  [[nodiscard]] std::string save() const {
    std::ostringstream out;
    out << seed << ' ' << rng;
    return out.str();
  }

  [[nodiscard]] static std::optional<RngStream> load(std::string_view text) {
    std::istringstream in{std::string(text)};
    RngStream stream(0u);
    in >> stream.seed >> stream.rng;
    if (in.fail()) {
      log_warn("RngStream::load: not a saved RngStream");
      return std::nullopt;
    }
    return stream;
  }

  [[nodiscard]] std::mt19937 &engine() { return rng; }

private:
  std::uint32_t seed = 0;
  std::mt19937 rng;
};

// Seedable global RNG. Unseeded it uses a random_device seed (nondeterministic);
// call set_seed() with a run/level seed to get a reproducible sequence.
//
// Name note (G-P12): `RandomEngine` collides with identically-named types in
// some consumers. Renaming it would be source-breaking for every in-repo and
// vendored caller, and the gap report prescribes no replacement name, so the
// rename is deferred — see CHANGELOG. The determinism fix above applies to
// RandomEngine through the RngStream it holds.
SINGLETON_FWD(RandomEngine)
struct RandomEngine {
  SINGLETON(RandomEngine)

  void set_seed(const std::string &new_seed) { rng.set_seed(new_seed); }
  void set_seed(std::uint32_t new_seed) { rng.set_seed(new_seed); }
  [[nodiscard]] std::uint32_t get_seed() const { return rng.get_seed(); }
  [[nodiscard]] std::uint32_t next_u32() { return rng.next_u32(); }
  [[nodiscard]] int get_int(int a, int b) { return rng.get_int(a, b); }
  [[nodiscard]] float get_float(float a, float b) {
    return rng.get_float(a, b);
  }
  [[nodiscard]] bool get_bool() { return rng.get_bool(); }
  template <typename T> [[nodiscard]] int get_index(const T &container) {
    return rng.get_index(container);
  }
  [[nodiscard]] std::mt19937 &engine() { return rng.engine(); }
  [[nodiscard]] RngStream &stream() { return rng; }

private:
  RngStream rng;
};

} // namespace afterhours
