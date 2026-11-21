#pragma once

// Backend-independent 4x4 helpers on MatrixType (raylib field layout).
//
//   MatrixType world = matrix_multiply(local, parent); // local applied first, then parent
//   MatrixType local = matrix_compose(pos, euler_deg, scale);

#include <cmath>

#include "../developer.h"

namespace afterhours {

inline MatrixType matrix_identity() {
  MatrixType m{};
  m.m0 = m.m5 = m.m10 = m.m15 = 1.f;
  return m;
}

// Same result as raylib's MatrixMultiply(a, b): a applies first, then b.
inline MatrixType matrix_multiply(const MatrixType &a, const MatrixType &b) {
  MatrixType r{};
  r.m0 = a.m0 * b.m0 + a.m1 * b.m4 + a.m2 * b.m8 + a.m3 * b.m12;
  r.m1 = a.m0 * b.m1 + a.m1 * b.m5 + a.m2 * b.m9 + a.m3 * b.m13;
  r.m2 = a.m0 * b.m2 + a.m1 * b.m6 + a.m2 * b.m10 + a.m3 * b.m14;
  r.m3 = a.m0 * b.m3 + a.m1 * b.m7 + a.m2 * b.m11 + a.m3 * b.m15;
  r.m4 = a.m4 * b.m0 + a.m5 * b.m4 + a.m6 * b.m8 + a.m7 * b.m12;
  r.m5 = a.m4 * b.m1 + a.m5 * b.m5 + a.m6 * b.m9 + a.m7 * b.m13;
  r.m6 = a.m4 * b.m2 + a.m5 * b.m6 + a.m6 * b.m10 + a.m7 * b.m14;
  r.m7 = a.m4 * b.m3 + a.m5 * b.m7 + a.m6 * b.m11 + a.m7 * b.m15;
  r.m8 = a.m8 * b.m0 + a.m9 * b.m4 + a.m10 * b.m8 + a.m11 * b.m12;
  r.m9 = a.m8 * b.m1 + a.m9 * b.m5 + a.m10 * b.m9 + a.m11 * b.m13;
  r.m10 = a.m8 * b.m2 + a.m9 * b.m6 + a.m10 * b.m10 + a.m11 * b.m14;
  r.m11 = a.m8 * b.m3 + a.m9 * b.m7 + a.m10 * b.m11 + a.m11 * b.m15;
  r.m12 = a.m12 * b.m0 + a.m13 * b.m4 + a.m14 * b.m8 + a.m15 * b.m12;
  r.m13 = a.m12 * b.m1 + a.m13 * b.m5 + a.m14 * b.m9 + a.m15 * b.m13;
  r.m14 = a.m12 * b.m2 + a.m13 * b.m6 + a.m14 * b.m10 + a.m15 * b.m14;
  r.m15 = a.m12 * b.m3 + a.m13 * b.m7 + a.m14 * b.m11 + a.m15 * b.m15;
  return r;
}

namespace detail_transform {
inline MatrixType axis_rot(int axis, float deg) {
  const float c = std::cos(deg * DEG2RAD), s = std::sin(deg * DEG2RAD);
  MatrixType m = matrix_identity();
  if (axis == 0) { m.m5 = c; m.m6 = s; m.m9 = -s; m.m10 = c; }
  if (axis == 1) { m.m0 = c; m.m2 = -s; m.m8 = s; m.m10 = c; }
  if (axis == 2) { m.m0 = c; m.m1 = s; m.m4 = -s; m.m5 = c; }
  return m;
}
} // namespace detail_transform

// Scale first, then Z, X, Y rotations (degrees), then translation.
inline MatrixType matrix_compose(Vector3Type pos, Vector3Type euler_deg,
                                 Vector3Type scale) {
  MatrixType s = matrix_identity();
  s.m0 = scale.x; s.m5 = scale.y; s.m10 = scale.z;
  MatrixType m = matrix_multiply(
      s, matrix_multiply(detail_transform::axis_rot(2, euler_deg.z),
                         matrix_multiply(detail_transform::axis_rot(0, euler_deg.x),
                                         detail_transform::axis_rot(1, euler_deg.y))));
  m.m12 = pos.x; m.m13 = pos.y; m.m14 = pos.z;
  return m;
}

inline Vector3Type transform_point(const MatrixType &m, Vector3Type p) {
  return {m.m0 * p.x + m.m4 * p.y + m.m8 * p.z + m.m12,
          m.m1 * p.x + m.m5 * p.y + m.m9 * p.z + m.m13,
          m.m2 * p.x + m.m6 * p.y + m.m10 * p.z + m.m14};
}

} // namespace afterhours
