#pragma once


#include <cmath>
#include <functional>

namespace afterhours::terrain_steer {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};

using HeightSampler = std::function<float(float x, float z)>;

constexpr float kEpsilon = 1e-5f;

inline Vec3 vec(float x, float y, float z) { return {x, y, z}; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline Vec3 scale(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }

inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }

inline Vec3 ground_normal(const HeightSampler &s, float x, float z,
                          float eps = 0.1f) {
  float dx = s(x + eps, z) - s(x - eps, z);
  float dz = s(x, z + eps) - s(x, z - eps);
  Vec3 n{-dx, 2.f * eps, -dz};
  float len = length(n);
  if (len < kEpsilon) return {0.f, 1.f, 0.f};
  return scale(n, 1.f / len);
}

inline float slope_degrees(Vec3 n) {
  float c = n.y < -1.f ? -1.f : (n.y > 1.f ? 1.f : n.y);
  return std::acos(c) * 57.29577951308232f;
}

inline bool can_climb(Vec3 n, float max_slope_deg) {
  return slope_degrees(n) <= max_slope_deg;
}

inline Vec3 align_velocity(Vec3 desired, Vec3 n) {
  return sub(desired, scale(n, dot(desired, n)));
}

inline Vec3 move_and_snap(Vec3 pos, Vec3 velocity, float dt,
                          const HeightSampler &s, float max_step) {
  Vec3 next{pos.x + velocity.x * dt, pos.y + velocity.y * dt,
            pos.z + velocity.z * dt};
  float ground_y = s(next.x, next.z);
  float dy = ground_y - next.y;
  float step = dy < -max_step ? -max_step : (dy > max_step ? max_step : dy);
  return {next.x, next.y + step, next.z};
}

inline Vec3 snap_to_ground(Vec3 pos, const HeightSampler &s, float max_step) {
  float ground_y = s(pos.x, pos.z);
  float dy = ground_y - pos.y;
  float step = dy < -max_step ? -max_step : (dy > max_step ? max_step : dy);
  return {pos.x, pos.y + step, pos.z};
}

}
