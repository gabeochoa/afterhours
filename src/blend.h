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
  CustomSeparate,   // separate RGB/alpha factors; see Separate
};

enum struct Factor {
  Zero,
  One,
  SrcColor,
  OneMinusSrcColor,
  SrcAlpha,
  OneMinusSrcAlpha,
  DstColor,
  OneMinusDstColor,
  DstAlpha,
  OneMinusDstAlpha,
  SrcAlphaSaturate,
};

enum struct Equation {
  Add,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

// Separate RGB and alpha blend factors and equations. Defaults are
// straight src-over, i.e. Mode::Alpha.
struct Separate {
  Factor src_rgb = Factor::SrcAlpha;
  Factor dst_rgb = Factor::OneMinusSrcAlpha;
  Factor src_alpha = Factor::One;
  Factor dst_alpha = Factor::OneMinusSrcAlpha;
  Equation rgb_eq = Equation::Add;
  Equation alpha_eq = Equation::Add;

  bool operator==(const Separate &) const = default;

  // Multiply the source into the destination RGB while leaving the
  // destination alpha untouched: compositing onto a render texture must
  // not punch alpha holes.
  static Separate multiply_preserving_alpha() {
    Separate s;
    s.src_rgb = Factor::DstColor;
    s.dst_rgb = Factor::Zero;
    s.src_alpha = Factor::Zero;
    s.dst_alpha = Factor::One;
    return s;
  }
};

namespace detail {

inline Mode &current_mode() {
  static Mode m = Mode::Alpha;
  return m;
}

inline Separate &current_separate() {
  static Separate s;
  return s;
}

inline std::size_t &transition_count() {
  static std::size_t n = 0;
  return n;
}

} // namespace detail

// What the backend is set to right now.
inline Mode current() { return detail::current_mode(); }

// The separate factors in effect when current() is CustomSeparate. When the
// mode is anything else this still holds the most recent separate setting, so
// a scope can restore it.
inline Separate current_separate() { return detail::current_separate(); }

// Real state changes since the last reset. A count that tracks the draw count
// means every draw is flushing the batch.
inline std::size_t transitions_this_frame() {
  return detail::transition_count();
}

inline void reset_transition_count() { detail::transition_count() = 0; }

} // namespace blend
} // namespace afterhours
