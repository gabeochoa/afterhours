#pragma once

#include "core/math3d.h"

#include <cstddef>
#include <vector>

namespace afterhours {
namespace skeletal {

using Vec3 = ::afterhours::Vec3;
using Quat = ::afterhours::Quat;
using Mat4 = ::afterhours::Mat4;
using TRS = ::afterhours::TRS;
using ::afterhours::lerp;
using ::afterhours::rotate;
using ::afterhours::slerp;
using ::afterhours::to_mat4;
using ::afterhours::transform_point;
using ::afterhours::translation_of;

struct Skeleton {
  std::vector<int> parent;
  std::vector<Mat4> inverse_bind;
};
struct Pose { std::vector<TRS> local; };

inline Pose bind_pose(std::size_t joints) {
  Pose p; p.local.assign(joints, TRS{}); return p;
}

inline void compute_world(const Skeleton &sk, const Pose &pose,
                          std::vector<Mat4> &out) {
  out.assign(sk.parent.size(), Mat4::identity());
  for (std::size_t i = 0; i < sk.parent.size(); i++) {
    Mat4 local = i < pose.local.size() ? to_mat4(pose.local[i]) : Mat4::identity();
    int p = sk.parent[i];
    out[i] = (p >= 0 && static_cast<std::size_t>(p) < i) ? out[p] * local : local;
  }
}

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
  float duration = 0.f;
  std::vector<JointTracks> joints;
};

namespace detail {
inline void segment(const std::vector<float> &times, float t, std::size_t &i,
                    float &f) {
  i = 0; f = 0.f;
  if (times.size() < 2 || t <= times.front()) return;
  if (t >= times.back()) { i = times.size() - 2; f = 1.f; return; }
  while (i + 2 < times.size() && times[i + 1] <= t) i++;
  float span = times[i + 1] - times[i];
  f = span > 0.f ? (t - times[i]) / span : 0.f;
}
}

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

inline void sample(const Clip &clip, float t, Pose &out) {
  for (std::size_t i = 0; i < clip.joints.size() && i < out.local.size(); i++) {
    const JointTracks &j = clip.joints[i];
    out.local[i].pos = sample(j.pos, t, out.local[i].pos);
    out.local[i].rot = sample(j.rot, t, out.local[i].rot);
    out.local[i].scale = sample(j.scale, t, out.local[i].scale);
  }
}

}
}
