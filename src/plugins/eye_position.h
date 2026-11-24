#pragma once

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
