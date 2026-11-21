#pragma once

// The eye position for draws and effects that need only where the viewer is,
// not a whole camera: fog and specular falloff, parallax impostors, LOD bias.
//
//   eye_position::set(camera.position);
//   apply_lighting(shader, lighting); // reads eye_position::get()
//
// A process-wide value, like blend::current: whatever set it last wins, and
// the default is the origin. Games that render from more than one viewpoint
// set it per pass (eye_position::Scope restores the previous one on exit).

#include "../developer.h"

namespace afterhours::eye_position {

namespace detail {
inline Vector3Type &storage() {
  static Vector3Type value{};
  return value;
}
} // namespace detail

// Where the viewer is, in world space. {0, 0, 0} until something sets it.
[[nodiscard]] inline Vector3Type get() { return detail::storage(); }

inline void set(Vector3Type pos) { detail::storage() = pos; }

// Back to the documented default, {0, 0, 0}.
inline void reset() { detail::storage() = {}; }

struct Scope {
  Vector3Type prev;
  explicit Scope(Vector3Type pos) : prev(get()) { set(pos); }
  ~Scope() { set(prev); }
};

} // namespace afterhours::eye_position
