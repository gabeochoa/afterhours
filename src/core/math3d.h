#pragma once

#include <cmath>

#include "../config.h"

#ifdef AFTER_HOURS_USE_RAYLIB
namespace raylib {
#include <raylib.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib
#endif

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB

using Vec3 = raylib::Vector3;

using Quat = raylib::Quaternion;

using Mat4 = raylib::Matrix;

inline Quat quat_identity() { return raylib::QuaternionIdentity(); }

inline Quat quat_axis_angle(Vec3 axis, float radians) {
  return raylib::QuaternionFromAxisAngle(axis, radians);
}

inline Mat4 mat4_identity() { return raylib::MatrixIdentity(); }

inline Vec3 lerp(Vec3 a, Vec3 b, float t) {
  return raylib::Vector3Lerp(a, b, t);
}

inline float dot(Vec3 a, Vec3 b) { return raylib::Vector3DotProduct(a, b); }

inline Vec3 cross(Vec3 a, Vec3 b) {
  return raylib::Vector3CrossProduct(a, b);
}

inline Quat normalize(Quat q) { return raylib::QuaternionNormalize(q); }

inline Quat multiply(Quat a, Quat b) {
  return normalize(raylib::QuaternionMultiply(a, b));
}

inline Vec3 rotate(Quat q, Vec3 v) {
  return raylib::Vector3RotateByQuaternion(v, raylib::QuaternionNormalize(q));
}

inline Quat slerp(Quat a, Quat b, float t) {
  return normalize(raylib::QuaternionSlerp(raylib::QuaternionNormalize(a),
                                             raylib::QuaternionNormalize(b),
                                             t));
}

inline Mat4 multiply(const Mat4 &a, const Mat4 &b) {
  return raylib::MatrixMultiply(b, a);
}

inline Vec3 transform_point(const Mat4 &m, Vec3 p) {
  return raylib::Vector3Transform(p, m);
}

inline Vec3 translation_of(const Mat4 &m) { return {m.m12, m.m13, m.m14}; }

struct TRS {
  Vec3 pos{};
  Quat rot{0.f, 0.f, 0.f, 1.f};
  Vec3 scale{1.f, 1.f, 1.f};
};

inline Mat4 to_mat4(const TRS &t) {
  raylib::Matrix s = raylib::MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  raylib::Matrix r =
      raylib::QuaternionToMatrix(raylib::QuaternionNormalize(t.rot));
  raylib::Matrix tr = raylib::MatrixTranslate(t.pos.x, t.pos.y, t.pos.z);
  return raylib::MatrixMultiply(s, raylib::MatrixMultiply(r, tr));
}

#else
// TODO: GLM for non-raylib backends
#endif

} // namespace afterhours
