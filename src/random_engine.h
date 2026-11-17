#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
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

  // Seed from an arbitrary string (e.g. a level code). Same string -> same
  // sequence.
  void set_seed(const std::string &new_seed) {
    set_seed(static_cast<std::uint32_t>(std::hash<std::string>{}(new_seed)));
  }
  void set_seed(std::uint32_t new_seed) {
    seed = new_seed;
    rng.seed(new_seed);
  }
  [[nodiscard]] std::uint32_t get_seed() const { return seed; }

  // Inclusive range [a, b]. std::uniform_int_distribution requires a <= b
  // (a > b is UB — can hang or return garbage). An inverted range is a caller
  // bug, so fail loud: log_error aborts in projects that wire it to assert,
  // surfacing the bad value immediately instead of papering over it. a == b is
  // a valid single-value range.
  [[nodiscard]] int get_int(int a, int b) {
    if (a > b)
      log_error("RngStream::get_int: inverted range [{}, {}] (a must be <= b)",
                a, b);
    return std::uniform_int_distribution<int>(a, b)(rng);
  }
  // Range [a, b). a > b is a caller bug (UB) -> fail loud; a == b is a valid
  // empty range that yields a.
  [[nodiscard]] float get_float(float a, float b) {
    if (a > b)
      log_error(
          "RngStream::get_float: inverted range [{}, {}) (a must be <= b)", a,
          b);
    return std::uniform_real_distribution<float>(a, b)(rng);
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
SINGLETON_FWD(RandomEngine)
struct RandomEngine {
  SINGLETON(RandomEngine)

  void set_seed(const std::string &new_seed) { rng.set_seed(new_seed); }
  void set_seed(std::uint32_t new_seed) { rng.set_seed(new_seed); }
  [[nodiscard]] std::uint32_t get_seed() const { return rng.get_seed(); }
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
