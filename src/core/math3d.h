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

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};

struct Quat {
  float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
  static Quat identity();
  static Quat axis_angle(Vec3 axis, float radians);
};

struct Mat4 {
  float m[16] = {};  // column-major: at(row, col) == m[col * 4 + row]
  static Mat4 identity();
  float at(int row, int col) const { return m[col * 4 + row]; }
};

namespace detail_math3d {
inline raylib::Vector3 to_rl(Vec3 v) { return {v.x, v.y, v.z}; }

inline Vec3 from_rl(raylib::Vector3 v) { return {v.x, v.y, v.z}; }

inline raylib::Quaternion to_rl(Quat q) { return {q.x, q.y, q.z, q.w}; }

inline Quat from_rl(raylib::Quaternion q) { return {q.x, q.y, q.z, q.w}; }

inline raylib::Matrix to_rl(const Mat4 &m) {
  raylib::Matrix r;
  r.m0 = m.m[0]; r.m1 = m.m[1]; r.m2 = m.m[2]; r.m3 = m.m[3];
  r.m4 = m.m[4]; r.m5 = m.m[5]; r.m6 = m.m[6]; r.m7 = m.m[7];
  r.m8 = m.m[8]; r.m9 = m.m[9]; r.m10 = m.m[10]; r.m11 = m.m[11];
  r.m12 = m.m[12]; r.m13 = m.m[13]; r.m14 = m.m[14]; r.m15 = m.m[15];
  return r;
}

inline Mat4 from_rl(const raylib::Matrix &r) {
  Mat4 m;
  m.m[0] = r.m0; m.m[1] = r.m1; m.m[2] = r.m2; m.m[3] = r.m3;
  m.m[4] = r.m4; m.m[5] = r.m5; m.m[6] = r.m6; m.m[7] = r.m7;
  m.m[8] = r.m8; m.m[9] = r.m9; m.m[10] = r.m10; m.m[11] = r.m11;
  m.m[12] = r.m12; m.m[13] = r.m13; m.m[14] = r.m14; m.m[15] = r.m15;
  return m;
}
} // namespace detail_math3d

inline Vec3 operator+(Vec3 a, Vec3 b) {
  return detail_math3d::from_rl(raylib::Vector3Add(detail_math3d::to_rl(a), detail_math3d::to_rl(b)));
}

inline Vec3 operator-(Vec3 a, Vec3 b) {
  return detail_math3d::from_rl(raylib::Vector3Subtract(detail_math3d::to_rl(a), detail_math3d::to_rl(b)));
}

inline Vec3 operator*(Vec3 a, float s) {
  return detail_math3d::from_rl(raylib::Vector3Scale(detail_math3d::to_rl(a), s));
}

inline Vec3 lerp(Vec3 a, Vec3 b, float t) {
  return detail_math3d::from_rl(raylib::Vector3Lerp(detail_math3d::to_rl(a), detail_math3d::to_rl(b), t));
}

inline float dot(Vec3 a, Vec3 b) {
  return raylib::Vector3DotProduct(detail_math3d::to_rl(a), detail_math3d::to_rl(b));
}

inline Vec3 cross(Vec3 a, Vec3 b) {
  return detail_math3d::from_rl(raylib::Vector3CrossProduct(detail_math3d::to_rl(a), detail_math3d::to_rl(b)));
}

inline Quat Quat::identity() { return detail_math3d::from_rl(raylib::QuaternionIdentity()); }

inline Quat Quat::axis_angle(Vec3 axis, float radians) {
  return detail_math3d::from_rl(raylib::QuaternionFromAxisAngle(detail_math3d::to_rl(axis), radians));
}

inline float dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

inline Quat normalize(Quat q) {
  return detail_math3d::from_rl(raylib::QuaternionNormalize(detail_math3d::to_rl(q)));
}

inline Quat operator*(Quat a, Quat b) {
  return normalize(detail_math3d::from_rl(
      raylib::QuaternionMultiply(detail_math3d::to_rl(a), detail_math3d::to_rl(b))));
}

inline Vec3 rotate(Quat q, Vec3 v) {
  return detail_math3d::from_rl(raylib::Vector3RotateByQuaternion(
      detail_math3d::to_rl(v), raylib::QuaternionNormalize(detail_math3d::to_rl(q))));
}

inline Quat slerp(Quat a, Quat b, float t) {
  return normalize(detail_math3d::from_rl(raylib::QuaternionSlerp(
      raylib::QuaternionNormalize(detail_math3d::to_rl(a)),
      raylib::QuaternionNormalize(detail_math3d::to_rl(b)), t)));
}

inline Mat4 Mat4::identity() { return detail_math3d::from_rl(raylib::MatrixIdentity()); }

inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
  // raymath MatrixMultiply applies its left operand first, so swap.
  return detail_math3d::from_rl(
      raylib::MatrixMultiply(detail_math3d::to_rl(b), detail_math3d::to_rl(a)));
}

inline Vec3 transform_point(const Mat4 &m, Vec3 p) {
  return detail_math3d::from_rl(raylib::Vector3Transform(detail_math3d::to_rl(p), detail_math3d::to_rl(m)));
}

inline Vec3 translation_of(const Mat4 &m) { return {m.m[12], m.m[13], m.m[14]}; }

struct TRS {
  Vec3 pos{};
  Quat rot{};
  Vec3 scale{1.f, 1.f, 1.f};
};

inline Mat4 to_mat4(const TRS &t) {
  raylib::Matrix s = raylib::MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  raylib::Matrix r = raylib::QuaternionToMatrix(raylib::QuaternionNormalize(detail_math3d::to_rl(t.rot)));
  raylib::Matrix tr = raylib::MatrixTranslate(t.pos.x, t.pos.y, t.pos.z);
  return detail_math3d::from_rl(raylib::MatrixMultiply(s, raylib::MatrixMultiply(r, tr)));
}

#else
// TODO: GLM for non-raylib backends
#endif

} // namespace afterhours
