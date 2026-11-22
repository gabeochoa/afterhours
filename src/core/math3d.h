#pragma once

#include <algorithm>
#include <cmath>

namespace afterhours {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 lerp(Vec3 a, Vec3 b, float t) { return a + (b - a) * t; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

struct Quat {
  float x = 0.f, y = 0.f, z = 0.f, w = 1.f;
  static Quat identity() { return {}; }
  static Quat axis_angle(Vec3 axis, float radians) {
    float len = std::sqrt(dot(axis, axis));
    if (len <= 0.f) return {};
    float s = std::sin(radians * 0.5f) / len;
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(radians * 0.5f)};
  }
};
inline float dot(Quat a, Quat b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline Quat normalize(Quat q) {
  float len = std::sqrt(dot(q, q));
  if (len <= 0.f) return {};
  float inv = 1.f / len;
  return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}
inline Quat operator*(Quat a, Quat b) {
  return normalize({a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z});
}
inline Vec3 rotate(Quat q, Vec3 v) {
  q = normalize(q);
  Vec3 u{q.x, q.y, q.z};
  return v + cross(u, cross(u, v) + v * q.w) * 2.f;
}
inline Quat slerp(Quat a, Quat b, float t) {
  a = normalize(a); b = normalize(b);
  float d = dot(a, b);
  if (d < 0.f) { b = {-b.x, -b.y, -b.z, -b.w}; d = -d; }
  if (d > 0.9995f)
    return normalize({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
  float theta = std::acos(std::clamp(d, -1.f, 1.f));
  float s = std::sin(theta);
  float wa = std::sin((1.f - t) * theta) / s, wb = std::sin(t * theta) / s;
  return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb,
          a.w * wa + b.w * wb};
}

struct Mat4 {
  float m[16] = {};  // column-major: at(row, col) == m[col * 4 + row]
  static Mat4 identity() {
    Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r;
  }
  float at(int row, int col) const { return m[col * 4 + row]; }
};
inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {
  Mat4 r;
  for (int c = 0; c < 4; c++)
    for (int row = 0; row < 4; row++) {
      float s = 0.f;
      for (int k = 0; k < 4; k++) s += a.at(row, k) * b.at(k, c);
      r.m[c * 4 + row] = s;
    }
  return r;
}
inline Vec3 transform_point(const Mat4 &m, Vec3 p) {
  return {m.at(0,0)*p.x + m.at(0,1)*p.y + m.at(0,2)*p.z + m.at(0,3),
          m.at(1,0)*p.x + m.at(1,1)*p.y + m.at(1,2)*p.z + m.at(1,3),
          m.at(2,0)*p.x + m.at(2,1)*p.y + m.at(2,2)*p.z + m.at(2,3)};
}
inline Vec3 translation_of(const Mat4 &m) { return {m.m[12], m.m[13], m.m[14]}; }

struct TRS {
  Vec3 pos{};
  Quat rot{};
  Vec3 scale{1.f, 1.f, 1.f};
};
inline Mat4 to_mat4(const TRS &t) {
  Quat q = normalize(t.rot);
  float x = q.x, y = q.y, z = q.z, w = q.w;
  Mat4 r = Mat4::identity();
  r.m[0] = (1.f - 2.f*(y*y + z*z)) * t.scale.x;
  r.m[1] = 2.f*(x*y + z*w) * t.scale.x;
  r.m[2] = 2.f*(x*z - y*w) * t.scale.x;
  r.m[4] = 2.f*(x*y - z*w) * t.scale.y;
  r.m[5] = (1.f - 2.f*(x*x + z*z)) * t.scale.y;
  r.m[6] = 2.f*(y*z + x*w) * t.scale.y;
  r.m[8] = 2.f*(x*z + y*w) * t.scale.z;
  r.m[9] = 2.f*(y*z - x*w) * t.scale.z;
  r.m[10] = (1.f - 2.f*(x*x + y*y)) * t.scale.z;
  r.m[12] = t.pos.x; r.m[13] = t.pos.y; r.m[14] = t.pos.z;
  return r;
}

} // namespace afterhours
