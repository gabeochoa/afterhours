#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

#include "../developer.h"

namespace afterhours::physics2d {

inline float approach(float current, float target, float max_delta) {
  max_delta = std::max(0.f, max_delta);
  if (current < target) return std::min(current + max_delta, target);
  return std::max(current - max_delta, target);
}

inline Vector2Type approach_vec(Vector2Type cur, Vector2Type target,
                                float max_delta) {
  float dx = target.x - cur.x;
  float dy = target.y - cur.y;
  float l = std::sqrt(dx * dx + dy * dy);
  if (l == 0.f || l <= max_delta) return target;
  float s = max_delta / l;
  return {cur.x + dx * s, cur.y + dy * s};
}

struct HorizontalParams {
  float max_speed = 200.f;
  float accel = 2000.f;
  float decel = 2500.f;
  float air_accel = 1200.f;
  float air_decel = 200.f;
  float turn_boost = 1.5f;
  float stop_snap = 5.f;
};

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

inline std::optional<float> intercept_time(Vector2Type from,
                                           Vector2Type target_pos,
                                           Vector2Type target_vel,
                                           float speed) {
  if (speed <= 0.f) return std::nullopt;
  float dx = target_pos.x - from.x;
  float dy = target_pos.y - from.y;
  float a = target_vel.x * target_vel.x + target_vel.y * target_vel.y -
            speed * speed;
  float b = 2.f * (dx * target_vel.x + dy * target_vel.y);
  float c = distance_sq(target_pos, from);
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

inline std::optional<Vector2Type> intercept_point(Vector2Type from,
                                                  Vector2Type target_pos,
                                                  Vector2Type target_vel,
                                                  float speed) {
  auto t = intercept_time(from, target_pos, target_vel, speed);
  if (!t) return std::nullopt;
  return Vector2Type{target_pos.x + target_vel.x * *t,
                     target_pos.y + target_vel.y * *t};
}

inline Vector2Type arrive_desired_velocity(Vector2Type pos, Vector2Type target,
                                           float max_speed, float slow_radius,
                                           float stop_radius = 0.f) {
  float dx = target.x - pos.x;
  float dy = target.y - pos.y;
  float dist = std::sqrt(dx * dx + dy * dy);
  if (dist <= stop_radius || dist == 0.f) return {};
  float speed = max_speed;
  if (slow_radius > stop_radius && dist < slow_radius)
    speed = max_speed * (dist - stop_radius) / (slow_radius - stop_radius);
  return {dx / dist * speed, dy / dist * speed};
}

inline Vector2Type arrive_velocity(Vector2Type pos, Vector2Type vel,
                                   Vector2Type target, float max_speed,
                                   float slow_radius, float max_accel, float dt,
                                   float stop_radius = 0.f) {
  Vector2Type desired =
      arrive_desired_velocity(pos, target, max_speed, slow_radius, stop_radius);
  return approach_vec(vel, desired, max_accel * dt);
}

struct Ellipse {
  Vector2Type center{};
  Vector2Type radii{1.f, 1.f};
};

inline float ellipse_metric(Vector2Type delta, Vector2Type combined_radii) {
  float sx = delta.x / std::max(combined_radii.x, 1e-6f);
  float sy = delta.y / std::max(combined_radii.y, 1e-6f);
  return sx * sx + sy * sy;
}

inline bool ellipses_overlap(Ellipse a, Ellipse b) {
  Vector2Type delta{b.center.x - a.center.x, b.center.y - a.center.y};
  Vector2Type radii{a.radii.x + b.radii.x, a.radii.y + b.radii.y};
  return ellipse_metric(delta, radii) <= 1.f;
}

struct SweepHit {
  float t = 0.f;
  Vector2Type normal{};
  Vector2Type point{};
};

inline std::optional<SweepHit> swept_ellipse(Ellipse moving,
                                             Vector2Type displacement,
                                             Ellipse fixed) {
  Vector2Type radii{moving.radii.x + fixed.radii.x,
                    moving.radii.y + fixed.radii.y};
  Vector2Type rel{moving.center.x - fixed.center.x,
                  moving.center.y - fixed.center.y};
  Vector2Type s0{rel.x / radii.x, rel.y / radii.y};
  Vector2Type sd{displacement.x / radii.x, displacement.y / radii.y};
  float a = sd.x * sd.x + sd.y * sd.y;
  float c = s0.x * s0.x + s0.y * s0.y - 1.f;
  if (c <= 0.f) {
    Vector2Type n{rel.x / (radii.x * radii.x), rel.y / (radii.y * radii.y)};
    float l = std::sqrt(n.x * n.x + n.y * n.y);
    if (l > 0.f) n = {n.x / l, n.y / l};
    return SweepHit{0.f, n, moving.center};
  }
  if (a == 0.f) return std::nullopt;
  float b = 2.f * (s0.x * sd.x + s0.y * sd.y);
  float disc = b * b - 4.f * a * c;
  if (disc < 0.f) return std::nullopt;
  float t = (-b - std::sqrt(disc)) / (2.f * a);
  if (t < 0.f || t > 1.f) return std::nullopt;
  Vector2Type centre{moving.center.x + displacement.x * t,
                     moving.center.y + displacement.y * t};
  Vector2Type delta{centre.x - fixed.center.x, centre.y - fixed.center.y};
  Vector2Type n{delta.x / (radii.x * radii.x), delta.y / (radii.y * radii.y)};
  float l = std::sqrt(n.x * n.x + n.y * n.y);
  if (l > 0.f) n = {n.x / l, n.y / l};
  return SweepHit{t, n, centre};
}

inline Vector2Type ellipse_separation(Ellipse a, Ellipse b) {
  if (!ellipses_overlap(a, b)) return {};
  Vector2Type radii{a.radii.x + b.radii.x, a.radii.y + b.radii.y};
  Vector2Type delta{a.center.x - b.center.x, a.center.y - b.center.y};
  if (delta.x * delta.x + delta.y * delta.y == 0.f) return {radii.x, 0.f};
  float m = std::sqrt(ellipse_metric(delta, radii));
  return {delta.x / m - delta.x, delta.y / m - delta.y};
}

} // namespace afterhours::physics2d
