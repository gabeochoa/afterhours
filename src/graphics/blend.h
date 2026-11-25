#pragma once

// Blend mode, plus a count of how often it really changed.
//
// Setting the mode already active does nothing, which matters because
// rlSetBlendMode flushes the GPU batch on every difference.

#include <cstddef>

namespace afterhours {
namespace blend {

enum struct Mode {
  Alpha,            // straight src-over, the default
  Additive,         // glow, light accumulation
  Multiplied,       // shadow, darkening
  AlphaPremultiply, // compositing a layer that already carries its alpha
};

namespace detail {
inline Mode &current_mode() {
  static Mode m = Mode::Alpha;
  return m;
}
inline std::size_t &transition_count() {
  static std::size_t n = 0;
  return n;
}
} // namespace detail

// What the backend is set to right now.
inline Mode current() { return detail::current_mode(); }

// Real state changes since the last reset. A count that tracks the draw count
// means every draw is flushing the batch.
inline std::size_t transitions_this_frame() {
  return detail::transition_count();
}
inline void reset_transition_count() { detail::transition_count() = 0; }

} // namespace blend
} // namespace afterhours
