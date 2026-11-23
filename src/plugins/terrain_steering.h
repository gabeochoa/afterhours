#pragma once

#include <algorithm>
#include <cmath>
#include <functional>

#include "../developer.h"

namespace afterhours::terrain_steer {

using HeightSampler = std::function<float(float x, float z)>;

inline Vector3Type ground_normal(const HeightSampler &s, float x, float z,
                                 float eps = 0.1f) {
  float dx = s(x + eps, z) - s(x - eps, z);
  float dz = s(x, z + eps) - s(x, z - eps);
  Vector3Type n{-dx, 2.f * eps, -dz};
  float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
  if (len < 1e-5f) return {0.f, 1.f, 0.f};
  return {n.x / len, n.y / len, n.z / len};
}

inline float slope_degrees(Vector3Type n) {
  return std::acos(std::clamp(n.y, -1.f, 1.f)) * RAD2DEG;
}

inline bool can_climb(Vector3Type n, float max_slope_deg) {
  return slope_degrees(n) <= max_slope_deg;
}

inline Vector3Type align_velocity(Vector3Type desired, Vector3Type n) {
  float d = desired.x * n.x + desired.y * n.y + desired.z * n.z;
  return {desired.x - n.x * d, desired.y - n.y * d, desired.z - n.z * d};
}

inline Vector3Type snap_to_ground(Vector3Type pos, const HeightSampler &s,
                                  float max_step) {
  float step = std::clamp(s(pos.x, pos.z) - pos.y, -max_step, max_step);
  return {pos.x, pos.y + step, pos.z};
}

inline Vector3Type move_and_snap(Vector3Type pos, Vector3Type velocity,
                                 float dt, const HeightSampler &s,
                                 float max_step) {
  Vector3Type next{pos.x + velocity.x * dt, pos.y + velocity.y * dt,
                   pos.z + velocity.z * dt};
  return snap_to_ground(next, s, max_step);
}

} // namespace afterhours::terrain_steer
