#pragma once

// Pure 2D movement/steering/collision math (Pumpkin V-W,Z). Std-only and
// backend-free: own Vec2 so callers need no host Vector2Type macro.

#include <algorithm>
#include <cmath>
#include <optional>

namespace afterhours {
namespace physics2d {

struct Vec2 {
  float x = 0.f, y = 0.f;
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float s) const { return {x * s, y * s}; }
  Vec2 operator/(float s) const { return {x / s, y / s}; }
};

inline float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float length_sq(Vec2 a) { return dot(a, a); }
inline float length(Vec2 a) { return std::sqrt(length_sq(a)); }
inline Vec2 normalized(Vec2 a) {
  float l = length(a);
  return l > 0.f ? a / l : Vec2{};
}

// Move `current` toward `target` by at most `max_delta` (max_delta >= 0).
inline float approach(float current, float target, float max_delta) {
  max_delta = std::max(0.f, max_delta);
  if (current < target) return std::min(current + max_delta, target);
  return std::max(current - max_delta, target);
}
inline Vec2 approach_vec(Vec2 cur, Vec2 target, float max_delta) {
  Vec2 d = target - cur;
  float l = length(d);
  if (l == 0.f || l <= max_delta) return target;
  return cur + d * (max_delta / l);
}

// --- V: horizontal character velocity -------------------------------------
struct HorizontalParams {
  float max_speed = 200.f;
  float accel = 2000.f;      // ground accel (units/s^2)
  float decel = 2500.f;      // ground decel when no input
  float air_accel = 1200.f;
  float air_decel = 200.f;
  float turn_boost = 1.5f;   // accel multiplier when reversing direction
  float stop_snap = 5.f;     // |v| below this with no input snaps to 0
};

// input in [-1,1]; returns new horizontal velocity after dt.
inline float horizontal_velocity(float vx, float input, float dt,
                                 const HorizontalParams &p, bool grounded) {
  input = std::clamp(input, -1.f, 1.f);
  float desired = input * p.max_speed;
  if (input == 0.f) {
    float next = approach(vx, 0.f, (grounded ? p.decel : p.air_decel) * dt);
    return (grounded && std::fabs(next) < p.stop_snap) ? 0.f : next;
  }
  float rate = grounded ? p.accel : p.air_accel;
  if (vx != 0.f && (vx > 0.f) != (desired > 0.f)) rate *= p.turn_boost;
  return approach(vx, desired, rate * dt);
}

// --- W: steering ------------------------------------------------------------
// Earliest t >= 0 with y + vy*t + 0.5*gravity*t^2 == target_y, if any.
// gravity is signed acceleration (e.g. -980 for downward in y-up space).
inline std::optional<float> time_until_height(float y, float vy, float target_y,
                                              float gravity) {
  float c = y - target_y;
  if (gravity == 0.f) {
    if (vy == 0.f) return c == 0.f ? std::optional<float>(0.f) : std::nullopt;
    float t = -c / vy;
    return t >= 0.f ? std::optional<float>(t) : std::nullopt;
  }
  float disc = vy * vy - 2.f * gravity * c;
  if (disc < 0.f) return std::nullopt;
  float sq = std::sqrt(disc);
  float best = -1.f;
  for (float t : {(-vy - sq) / gravity, (-vy + sq) / gravity})
    if (t >= 0.f && (best < 0.f || t < best)) best = t;
  return best >= 0.f ? std::optional<float>(best) : std::nullopt;
}

// Time for a projectile of `speed` from `from` to hit a target moving at
// constant `target_vel`; nullopt if unreachable / speed <= 0.
inline std::optional<float> intercept_time(Vec2 from, Vec2 target_pos,
                                           Vec2 target_vel, float speed) {
  if (speed <= 0.f) return std::nullopt;
  Vec2 d = target_pos - from;
  float a = length_sq(target_vel) - speed * speed;
  float b = 2.f * dot(d, target_vel);
  float c = length_sq(d);
  if (c == 0.f) return 0.f;
  if (std::fabs(a) < 1e-6f) {
    if (std::fabs(b) < 1e-6f) return std::nullopt;
    float t = -c / b;
    return t >= 0.f ? std::optional<float>(t) : std::nullopt;
  }
  float disc = b * b - 4.f * a * c;
  if (disc < 0.f) return std::nullopt;
  float sq = std::sqrt(disc), best = -1.f;
  for (float t : {(-b - sq) / (2.f * a), (-b + sq) / (2.f * a)})
    if (t >= 0.f && (best < 0.f || t < best)) best = t;
  return best >= 0.f ? std::optional<float>(best) : std::nullopt;
}

inline std::optional<Vec2> intercept_point(Vec2 from, Vec2 target_pos,
                                           Vec2 target_vel, float speed) {
  auto t = intercept_time(from, target_pos, target_vel, speed);
  return t ? std::optional<Vec2>(target_pos + target_vel * *t) : std::nullopt;
}

// Desired velocity for braking arrival: full speed outside slow_radius,
// linearly slower inside, 0 at/within stop_radius.
inline Vec2 arrive_desired_velocity(Vec2 pos, Vec2 target, float max_speed,
                                    float slow_radius, float stop_radius = 0.f) {
  Vec2 d = target - pos;
  float dist = length(d);
  if (dist <= stop_radius || dist == 0.f) return {};
  float speed = max_speed;
  if (slow_radius > stop_radius && dist < slow_radius)
    speed = max_speed * (dist - stop_radius) / (slow_radius - stop_radius);
  return d / dist * speed;
}

// One steering step toward braking arrival, accel-limited; decelerates to 0
// at the target instead of oscillating (velocity-scaled stop via desired=0).
inline Vec2 arrive_velocity(Vec2 pos, Vec2 vel, Vec2 target, float max_speed,
                            float slow_radius, float max_accel, float dt,
                            float stop_radius = 0.f) {
  Vec2 desired = arrive_desired_velocity(pos, target, max_speed, slow_radius, stop_radius);
  return approach_vec(vel, desired, max_accel * dt);
}

// --- Z: ellipse collision (combined-radii model) ----------------------------
// Overlap iff ((dx)/(ax+bx))^2 + ((dy)/(ay+by))^2 <= 1. Exact for a point vs
// an ellipse; a documented approximation for two ellipses.
struct Ellipse {
  Vec2 center{};
  Vec2 radii{1.f, 1.f};
};

inline float ellipse_metric(Vec2 delta, Vec2 combined_radii) {
  Vec2 s = {delta.x / std::max(combined_radii.x, 1e-6f),
            delta.y / std::max(combined_radii.y, 1e-6f)};
  return length_sq(s);
}
inline bool ellipses_overlap(Ellipse a, Ellipse b) {
  return ellipse_metric(b.center - a.center, {a.radii.x + b.radii.x, a.radii.y + b.radii.y}) <= 1.f;
}

struct SweepHit {
  float t = 0.f; // in [0,1] along displacement
  Vec2 normal{}; // from static toward moving, normalized
  Vec2 point{};  // moving centre at contact
};

// Earliest contact of `moving` swept by `displacement` against `fixed`.
inline std::optional<SweepHit> swept_ellipse(Ellipse moving, Vec2 displacement,
                                             Ellipse fixed) {
  Vec2 radii = {moving.radii.x + fixed.radii.x, moving.radii.y + fixed.radii.y};
  Vec2 rel = moving.center - fixed.center;
  // Solve |((rel + d t) / radii)|^2 == 1.
  Vec2 s0 = {rel.x / radii.x, rel.y / radii.y};
  Vec2 sd = {displacement.x / radii.x, displacement.y / radii.y};
  float a = length_sq(sd), c = length_sq(s0) - 1.f;
  if (c <= 0.f) { // already overlapping
    Vec2 n = normalized({rel.x / (radii.x * radii.x), rel.y / (radii.y * radii.y)});
    return SweepHit{0.f, n, moving.center};
  }
  if (a == 0.f) return std::nullopt;
  float b = 2.f * dot(s0, sd);
  float disc = b * b - 4.f * a * c;
  if (disc < 0.f) return std::nullopt;
  float t = (-b - std::sqrt(disc)) / (2.f * a);
  if (t < 0.f || t > 1.f) return std::nullopt;
  Vec2 centre = moving.center + displacement * t;
  Vec2 delta = centre - fixed.center;
  Vec2 n = normalized({delta.x / (radii.x * radii.x), delta.y / (radii.y * radii.y)});
  return SweepHit{t, n, centre};
}

// Minimum translation to push `a` out of `b` ({} if not overlapping).
inline Vec2 ellipse_separation(Ellipse a, Ellipse b) {
  if (!ellipses_overlap(a, b)) return {};
  Vec2 radii = {a.radii.x + b.radii.x, a.radii.y + b.radii.y};
  Vec2 delta = a.center - b.center;
  if (length_sq(delta) == 0.f) return {radii.x, 0.f}; // concentric: push +x out
  float m = std::sqrt(ellipse_metric(delta, radii)); // <= 1
  // Scale delta up to the boundary; gradient direction is the push axis.
  Vec2 target = delta / m;
  return target - delta;
}

} // namespace physics2d
} // namespace afterhours
