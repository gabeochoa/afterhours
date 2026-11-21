#pragma once

// Skeletal animation sampling math only: local TRS pose -> world matrices ->
// skinning matrices (world * inverse-bind). No renderer, no skin import.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace afterhours {
namespace skeletal {

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
inline Quat operator*(Quat a, Quat b) {  // apply b, then a
  return normalize({a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z});
}
inline Vec3 rotate(Quat q, Vec3 v) {
  q = normalize(q);
  Vec3 u{q.x, q.y, q.z};
  // v + 2*cross(u, cross(u,v) + w*v)
  return v + cross(u, cross(u, v) + v * q.w) * 2.f;
}
inline Quat slerp(Quat a, Quat b, float t) {
  a = normalize(a); b = normalize(b);
  float d = dot(a, b);
  if (d < 0.f) { b = {-b.x, -b.y, -b.z, -b.w}; d = -d; }  // shortest path
  if (d > 0.9995f)  // nearly parallel: nlerp avoids the sin(0) divide
    return normalize({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
  float theta = std::acos(std::clamp(d, -1.f, 1.f));
  float s = std::sin(theta);
  float wa = std::sin((1.f - t) * theta) / s, wb = std::sin(t * theta) / s;
  return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb,
          a.w * wa + b.w * wb};
}

// Column-major: m[col*4 + row], so m[12..14] is translation (OpenGL/raylib style).
struct Mat4 {
  float m[16] = {};
  static Mat4 identity() {
    Mat4 r; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r;
  }
  float at(int row, int col) const { return m[col * 4 + row]; }
};
inline Mat4 operator*(const Mat4 &a, const Mat4 &b) {  // apply b, then a
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
  // Rotation columns, each scaled by the matching scale component.
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

struct Skeleton {
  std::vector<int> parent;        // -1 = root; parents must precede children
  std::vector<Mat4> inverse_bind; // empty = identity per joint
};
struct Pose { std::vector<TRS> local; };

inline Pose bind_pose(std::size_t joints) {
  Pose p; p.local.assign(joints, TRS{}); return p;
}

// World matrices down the tree; `out` is resized to the joint count.
inline void compute_world(const Skeleton &sk, const Pose &pose,
                          std::vector<Mat4> &out) {
  out.assign(sk.parent.size(), Mat4::identity());
  for (std::size_t i = 0; i < sk.parent.size(); i++) {
    Mat4 local = i < pose.local.size() ? to_mat4(pose.local[i]) : Mat4::identity();
    int p = sk.parent[i];
    out[i] = (p >= 0 && static_cast<std::size_t>(p) < i) ? out[p] * local : local;
  }
}

// Skinning matrix per joint: world * inverse-bind (identity bind if absent).
inline void compute_skinning(const Skeleton &sk,
                             const std::vector<Mat4> &world,
                             std::vector<Mat4> &out) {
  out.assign(world.size(), Mat4::identity());
  for (std::size_t i = 0; i < world.size(); i++)
    out[i] = world[i] * (i < sk.inverse_bind.size() ? sk.inverse_bind[i]
                                                    : Mat4::identity());
}

struct Vec3Track { std::vector<float> times; std::vector<Vec3> values; };
struct QuatTrack { std::vector<float> times; std::vector<Quat> values; };
struct JointTracks { Vec3Track pos; QuatTrack rot; Vec3Track scale; };
struct Clip {
  float duration = 0.f;  // informational; sampling clamps to the key range
  std::vector<JointTracks> joints;  // per joint; missing/empty = bind value
};

namespace detail {
// Segment [i, i+1] and blend factor for t; clamps outside the key range.
inline void segment(const std::vector<float> &times, float t, std::size_t &i,
                    float &f) {
  i = 0; f = 0.f;
  if (times.size() < 2 || t <= times.front()) return;
  if (t >= times.back()) { i = times.size() - 2; f = 1.f; return; }
  while (i + 2 < times.size() && times[i + 1] <= t) i++;
  float span = times[i + 1] - times[i];
  f = span > 0.f ? (t - times[i]) / span : 0.f;
}
}  // namespace detail

inline Vec3 sample(const Vec3Track &tr, float t, Vec3 fallback) {
  if (tr.values.empty()) return fallback;
  if (tr.values.size() == 1) return tr.values[0];
  std::size_t i; float f;
  detail::segment(tr.times, t, i, f);
  return lerp(tr.values[i], tr.values[i + 1], f);
}
inline Quat sample(const QuatTrack &tr, float t, Quat fallback) {
  if (tr.values.empty()) return fallback;
  if (tr.values.size() == 1) return tr.values[0];
  std::size_t i; float f;
  detail::segment(tr.times, t, i, f);
  return slerp(tr.values[i], tr.values[i + 1], f);
}

// Sample every joint at t into `out` (started from `base`, usually bind pose).
inline void sample(const Clip &clip, float t, Pose &out) {
  for (std::size_t i = 0; i < clip.joints.size() && i < out.local.size(); i++) {
    const JointTracks &j = clip.joints[i];
    out.local[i].pos = sample(j.pos, t, out.local[i].pos);
    out.local[i].rot = sample(j.rot, t, out.local[i].rot);
    out.local[i].scale = sample(j.scale, t, out.local[i].scale);
  }
}

}  // namespace skeletal
}  // namespace afterhours
