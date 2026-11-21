#pragma once


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

  void set_seed(const std::string &new_seed) {
    std::uint32_t hash = 2166136261u;
    for (char c : new_seed) {
      hash ^= static_cast<unsigned char>(c);
      hash *= 16777619u;
    }
    set_seed(hash);
  }
  void set_seed(std::uint32_t new_seed) {
    seed = new_seed;
    rng.seed(new_seed);
  }
  [[nodiscard]] std::uint32_t get_seed() const { return seed; }

  [[nodiscard]] std::uint32_t next_u32() { return rng(); }

  [[nodiscard]] int get_int(int a, int b) {
    if (a > b)
      log_error("RngStream::get_int: inverted range [{}, {}] (a must be <= b)",
                a, b);
    const std::int64_t span = static_cast<std::int64_t>(b) -
                              static_cast<std::int64_t>(a) + 1;
    std::int64_t offset = 0;
    if (span == (std::int64_t{1} << 32)) {
      offset = static_cast<std::int64_t>(next_u32());
    } else {
      const std::uint32_t n = static_cast<std::uint32_t>(span);
      std::uint32_t draw = next_u32();
      std::uint64_t mixed = static_cast<std::uint64_t>(draw) * n;
      std::uint32_t low = static_cast<std::uint32_t>(mixed);
      if (low < n) {
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
  [[nodiscard]] float get_float(float a, float b) {
    if (a > b)
      log_error(
          "RngStream::get_float: inverted range [{}, {}) (a must be <= b)", a,
          b);
    const double unit =
        static_cast<double>(next_u32() >> 8) * (1.0 / 16777216.0);
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
