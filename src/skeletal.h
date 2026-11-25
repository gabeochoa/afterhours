#pragma once

#include "plugins/math3d.h"

#include <cstddef>
#include <vector>

namespace afterhours {
namespace skeletal {

#ifdef AFTER_HOURS_USE_RAYLIB

using Vec3 = ::afterhours::Vec3;
using Quat = ::afterhours::Quat;
using Mat4 = ::afterhours::Mat4;
using TRS = ::afterhours::TRS;
using ::afterhours::lerp;
using ::afterhours::mat4_identity;
using ::afterhours::multiply;
using ::afterhours::quat_axis_angle;
using ::afterhours::quat_identity;
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
  out.assign(sk.parent.size(), mat4_identity());
  for (std::size_t i = 0; i < sk.parent.size(); i++) {
    Mat4 local = i < pose.local.size() ? to_mat4(pose.local[i]) : mat4_identity();
    int p = sk.parent[i];
    out[i] = (p >= 0 && static_cast<std::size_t>(p) < i) ? multiply(out[p], local) : local;
  }
}

inline void compute_skinning(const Skeleton &sk,
                             const std::vector<Mat4> &world,
                             std::vector<Mat4> &out) {
  out.assign(world.size(), mat4_identity());
  for (std::size_t i = 0; i < world.size(); i++)
    out[i] = multiply(world[i], i < sk.inverse_bind.size()
                                    ? sk.inverse_bind[i]
                                    : mat4_identity());
}

template <typename T>
struct Track { std::vector<float> times; std::vector<T> values; };

using Vec3Track = Track<Vec3>;
using QuatTrack = Track<Quat>;

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
inline Vec3 interpolate(Vec3 a, Vec3 b, float f) { return lerp(a, b, f); }
inline Quat interpolate(Quat a, Quat b, float f) { return slerp(a, b, f); }
}

template <typename T>
inline T sample(const Track<T> &tr, float t, T fallback) {
  if (tr.values.empty()) return fallback;
  if (tr.values.size() == 1) return tr.values[0];
  std::size_t i; float f;
  detail::segment(tr.times, t, i, f);
  return detail::interpolate(tr.values[i], tr.values[i + 1], f);
}

inline void sample(const Clip &clip, float t, Pose &out) {
  for (std::size_t i = 0; i < clip.joints.size() && i < out.local.size(); i++) {
    const JointTracks &j = clip.joints[i];
    out.local[i].pos = sample(j.pos, t, out.local[i].pos);
    out.local[i].rot = sample(j.rot, t, out.local[i].rot);
    out.local[i].scale = sample(j.scale, t, out.local[i].scale);
  }
}

#else
// TODO: GLM for non-raylib backends
#endif

} // namespace skeletal
} // namespace afterhours
