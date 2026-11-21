#pragma once
#include <cmath>
#include <functional>
namespace afterhours {
struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
  Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  [[nodiscard]] float length() const {
    return std::sqrt(x * x + y * y + z * z);
  }
};
using HeightSampler = std::function<float(float x, float z)>;
struct GroundHit {
  float height = 0.f;
  Vec3 normal{0.f, 1.f, 0.f};
  float slope_deg = 0.f;
  bool walkable = false;
};
inline GroundHit sample_ground(const HeightSampler &sample, float x, float z,
                               float max_slope_deg, float eps = 0.1f) {
  GroundHit hit;
  hit.height = sample(x, z);
  float dx = sample(x + eps, z) - sample(x - eps, z);
  float dz = sample(x, z + eps) - sample(x, z - eps);
  Vec3 n{-dx / (2.f * eps), 1.f, -dz / (2.f * eps)};
  float len = n.length();
  hit.normal = len > 0.f ? n * (1.f / len) : Vec3{0.f, 1.f, 0.f};
  float cos_slope = hit.normal.y < -1.f ? -1.f : (hit.normal.y > 1.f ? 1.f : hit.normal.y);
  hit.slope_deg = std::acos(cos_slope) * 57.29577951308232f;
  hit.walkable = hit.slope_deg <= max_slope_deg;
  return hit;
}
struct CharacterMotor {
  Vec3 position{};
  Vec3 velocity{};
  bool grounded = false;
  Vec3 ground_normal{0.f, 1.f, 0.f};
  float max_slope_deg = 50.f;
  float step_height = 0.55f;
  float snap_distance = 0.6f;
  float gravity = -30.f;
  float ground_stick = 2.f;
  void update(float dt, Vec3 wish_dir, float speed,
              const HeightSampler &sample) {
    if (dt <= 0.f || !sample) return;
    float wish_len = std::sqrt(wish_dir.x * wish_dir.x + wish_dir.z * wish_dir.z);
    Vec3 dir = wish_len > 0.f
                   ? Vec3{wish_dir.x / wish_len, 0.f, wish_dir.z / wish_len}
                   : Vec3{};
    Vec3 step = dir * (speed * dt);
    velocity.x = dir.x * speed;
    velocity.z = dir.z * speed;
    if (grounded) {
      GroundHit here = sample_ground(sample, position.x, position.z, max_slope_deg);
      if (position.y - here.height > snap_distance) {
        grounded = false;
      } else {
        position.y = here.height;
        ground_normal = here.normal;
        velocity.y = -ground_stick;
      }
    }
    if (grounded) {
      Vec3 next = position + step;
      GroundHit cand = sample_ground(sample, next.x, next.z, max_slope_deg);
      float rise = cand.height - position.y;
      if (rise > 0.f) {
        if (rise <= step_height) {
          Vec3 probe{next.x + dir.x * 0.15f, 0.f, next.z + dir.z * 0.15f};
          GroundHit ahead = sample_ground(sample, probe.x, probe.z, max_slope_deg);
          if (ahead.walkable && ahead.height - cand.height <= step_height) {
            position = {next.x, cand.height, next.z};
            ground_normal = cand.normal;
          }
        }
      } else if (position.y - cand.height <= snap_distance) {
        if (cand.walkable) {
          position = {next.x, cand.height, next.z};
          ground_normal = cand.normal;
        } else {
          grounded = false;
        }
      } else {
        grounded = false;
      }
    }
    if (!grounded) {
      velocity.y += gravity * dt;
      GroundHit ahead = sample_ground(sample, position.x + step.x,
                                      position.z + step.z, max_slope_deg);
      if (ahead.height <= position.y + step_height) {
        position.x += step.x;
        position.z += step.z;
      }
      position.y += velocity.y * dt;
      GroundHit below = sample_ground(sample, position.x, position.z, max_slope_deg);
      if (position.y <= below.height) {
        position.y = below.height;
        velocity.y = 0.f;
        if (below.walkable) {
          grounded = true;
          ground_normal = below.normal;
        }
      }
    }
  }
};
}
