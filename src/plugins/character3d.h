#pragma once

// The smallest 3D character motor: walk on a heightfield, nothing else.
//
//   CharacterMotor motor;            // spawn it resting on the ground
//   motor.position = {0.f, terrain(0.f, 0.f), 0.f};
//   motor.update(dt, wish_dir, 4.f, [](float x, float z) { return terrain(x, z); });
//
// The ground is a caller-owned height sampler, `float(float x, float z)`, so
// this composes with any terrain (a Heightfield when that lands, a game's own
// heightmap today) and stays testable headless. Movement is slope-limited
// ground movement with ground snap and step-up: ground steeper than
// `max_slope_deg` refuses uphill movement, a ledge no taller than
// `step_height` is climbed, and walking off an edge falls under gravity until
// the ground catches the motor again.
//
// Deferred (R-187, the collision-world slice): mantle, ladders and ropes,
// headroom/overhead checks, and any collision beyond the heightfield (walls,
// AABB/OBB/cylinder push-out). Slope slide is also left there: landing on
// too-steep ground leaves the motor resting on it, ungrounded. Until then the
// motor is a point - radius and capsule height are the collision slice's to
// add, and callers should keep paths a body-width clear of walls themselves.

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
  float height = 0.f;               // ground height at the sampled point
  Vec3 normal{0.f, 1.f, 0.f};       // up-facing ground normal
  float slope_deg = 0.f;            // 0 flat, 90 a cliff face
  bool walkable = false;            // slope_deg <= the queried limit
};

// Ground query: height, normal (central differences over `eps`) and slope at
// (x, z). `walkable` is judged against max_slope_deg here so movement and any
// caller's own checks read the same verdict.
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
  hit.slope_deg = std::acos(cos_slope) * 57.29577951308232f; // rad -> deg
  hit.walkable = hit.slope_deg <= max_slope_deg;
  return hit;
}

struct CharacterMotor {
  Vec3 position{};
  Vec3 velocity{}; // horizontal wish velocity, plus vertical fall speed
  bool grounded = false;
  Vec3 ground_normal{0.f, 1.f, 0.f};

  float max_slope_deg = 50.f;
  float step_height = 0.55f;   // tallest ledge climbed without jumping
  float snap_distance = 0.6f;  // tallest drop still glued to the ground
  float gravity = -30.f;       // units/s^2 while airborne
  float ground_stick = 2.f;    // downward speed held while grounded

  // Advance one step. wish_dir's y is ignored; pass a zero vector to stand
  // still. Deterministic in dt: same inputs, same path, no hidden clock.
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
        grounded = false; // walked off an edge, or the ground fell away
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
        // Uphill or a ledge: climb only within step height, and judge the
        // slope just past the candidate - a ledge edge itself reads as a
        // cliff to the normal, however flat the top is.
        if (rise <= step_height) {
          Vec3 probe{next.x + dir.x * 0.15f, 0.f, next.z + dir.z * 0.15f};
          GroundHit ahead = sample_ground(sample, probe.x, probe.z, max_slope_deg);
          if (ahead.walkable && ahead.height - cand.height <= step_height) {
            position = {next.x, cand.height, next.z};
            ground_normal = cand.normal;
          }
        } // else refused: too steep, or a ledge taller than a step
      } else if (position.y - cand.height <= snap_distance) {
        if (cand.walkable) {
          position = {next.x, cand.height, next.z}; // snap down, staying grounded
          ground_normal = cand.normal;
        } else {
          grounded = false; // downhill onto too-steep ground: let go and fall
        }
      } else {
        grounded = false; // walked off an edge; the fall below moves us
      }
    }

    if (!grounded) {
      velocity.y += gravity * dt;
      // A wall taller than a step blocks mid-fall too; without this the
      // landing snap below would pull the motor up onto it.
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
        } // else resting on too-steep ground, ungrounded (slide is R-187)
      }
    }
  }
};

} // namespace afterhours
