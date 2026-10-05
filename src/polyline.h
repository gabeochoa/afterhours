#pragma once

#include <cmath>
#include <numbers>
#include <vector>

#include "drawing_helpers.h"

namespace afterhours {

// draw_line_ex draws a solid segment and there was nothing patterned, so a
// dashed run had to be hand-rolled: accumulate arc length along the polyline
// and emit a segment when it falls inside a dash window. puzzle did that for
// live patch cables and wants the same for selection marquees and timeline
// ticks.
//
// Everything here is geometry over draw_line_ex, so it works on any backend.
namespace polyline {

inline float distance(const Vector2Type &a, const Vector2Type &b) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

inline Vector2Type lerp(const Vector2Type &a, const Vector2Type &b, float t) {
  return Vector2Type{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

inline float total_length(const std::vector<Vector2Type> &pts) {
  float len = 0.f;
  for (size_t i = 0; i + 1 < pts.size(); i++)
    len += distance(pts[i], pts[i + 1]);
  return len;
}

enum class ChevronDirection { Right, Down, Left, Up };

struct ChevronPoints {
  Vector2Type first;
  Vector2Type tip;
  Vector2Type second;
};

inline float chevron_direction_radians(ChevronDirection direction) {
  constexpr float pi = std::numbers::pi_v<float>;
  switch (direction) {
  case ChevronDirection::Right:
    return 0.f;
  case ChevronDirection::Down:
    return pi * 0.5f;
  case ChevronDirection::Left:
    return pi;
  case ChevronDirection::Up:
    return -pi * 0.5f;
  }
  return 0.f;
}

// Screen coordinates: y is down and positive angles turn clockwise. The tip
// points along direction_radians; both arms trail behind it by half_open.
inline ChevronPoints chevron_points(Vector2Type tip, float arm,
                                    float direction_radians,
                                    float half_open_radians) {
  if (arm <= 0.f)
    return ChevronPoints{tip, tip, tip};
  const float back = direction_radians + std::numbers::pi_v<float>;
  const auto endpoint = [&](float angle) {
    return Vector2Type{tip.x + arm * std::cos(angle),
                       tip.y + arm * std::sin(angle)};
  };
  return ChevronPoints{endpoint(back - half_open_radians), tip,
                       endpoint(back + half_open_radians)};
}

inline ChevronPoints chevron_points(
    Vector2Type tip, float arm, ChevronDirection direction,
    float half_open_radians = std::numbers::pi_v<float> * 0.25f) {
  return chevron_points(tip, arm, chevron_direction_radians(direction),
                        half_open_radians);
}

inline ChevronPoints chevron_points_in_rect(RectangleType bounds,
                                            ChevronDirection direction) {
  const float left = bounds.x;
  const float right = bounds.x + bounds.width;
  const float top = bounds.y;
  const float bottom = bounds.y + bounds.height;
  const float cx = bounds.x + bounds.width * 0.5f;
  const float cy = bounds.y + bounds.height * 0.5f;
  switch (direction) {
  case ChevronDirection::Right:
    return ChevronPoints{Vector2Type{left, bottom}, Vector2Type{right, cy},
                         Vector2Type{left, top}};
  case ChevronDirection::Down:
    return ChevronPoints{Vector2Type{left, top}, Vector2Type{cx, bottom},
                         Vector2Type{right, top}};
  case ChevronDirection::Left:
    return ChevronPoints{Vector2Type{right, top}, Vector2Type{left, cy},
                         Vector2Type{right, bottom}};
  case ChevronDirection::Up:
    return ChevronPoints{Vector2Type{left, bottom}, Vector2Type{cx, top},
                         Vector2Type{right, bottom}};
  }
  return chevron_points_in_rect(bounds, ChevronDirection::Right);
}

inline void draw_chevron(const ChevronPoints &points, float thickness,
                         const Color color) {
  if (thickness <= 0.f)
    return;
  if (distance(points.first, points.tip) <= 0.f ||
      distance(points.tip, points.second) <= 0.f)
    return;
  draw_line_ex(points.first, points.tip, thickness, color);
  draw_line_ex(points.tip, points.second, thickness, color);
}

inline void draw_chevron(Vector2Type tip, float arm, float direction_radians,
                         float half_open_radians, float thickness,
                         const Color color) {
  draw_chevron(chevron_points(tip, arm, direction_radians, half_open_radians),
               thickness, color);
}

inline void draw_chevron(
    Vector2Type tip, float arm, ChevronDirection direction, float thickness,
    const Color color,
    float half_open_radians = std::numbers::pi_v<float> * 0.25f) {
  draw_chevron(chevron_points(tip, arm, direction, half_open_radians),
               thickness, color);
}

inline void draw_chevron_in_rect(RectangleType bounds,
                                 ChevronDirection direction, float thickness,
                                 const Color color) {
  draw_chevron(chevron_points_in_rect(bounds, direction), thickness, color);
}

// Evenly spaced points along the curve. A bezier sampler walks uniformly in t,
// which is not uniform in screen distance, so points bunch where the curve is
// tight and anything spaced along the result inherits that: dashes, arrowheads,
// flow pulses, labels.
inline std::vector<Vector2Type>
resample_by_arclength(const std::vector<Vector2Type> &pts, float step) {
  std::vector<Vector2Type> out;
  if (pts.size() < 2 || step <= 0.f)
    return pts;

  out.push_back(pts.front());
  float carry = 0.f; // distance already covered toward the next output point
  for (size_t i = 0; i + 1 < pts.size(); i++) {
    const Vector2Type a = pts[i];
    const Vector2Type b = pts[i + 1];
    const float seg = distance(a, b);
    if (seg <= 0.f)
      continue;
    float t = step - carry;
    while (t <= seg) {
      out.push_back(lerp(a, b, t / seg));
      t += step;
    }
    carry = seg - (t - step);
  }
  // The last point is kept whatever the spacing, or the curve visibly stops
  // short of where it ends.
  if (distance(out.back(), pts.back()) > 0.f)
    out.push_back(pts.back());
  return out;
}

// `phase` shifts the pattern along the line, which is what animates it, and is
// easy to leave out of a hand-rolled version. Arc length accumulates across
// vertices rather than restarting at each, so a dash straddling a corner stays
// one dash.
inline void draw_dashed(const std::vector<Vector2Type> &pts, float thickness,
                        const Color color, float dash, float gap,
                        float phase = 0.f) {
  if (pts.size() < 2 || dash <= 0.f || gap < 0.f)
    return;

  const float period = dash + gap;
  float in_period = std::fmod(phase, period);
  if (in_period < 0.f)
    in_period += period;
  bool inked = in_period < dash;
  float remaining = inked ? dash - in_period : period - in_period;

  for (size_t i = 0; i + 1 < pts.size(); i++) {
    const Vector2Type a = pts[i];
    const Vector2Type b = pts[i + 1];
    const float seg = distance(a, b);
    if (seg <= 0.f)
      continue;

    float t = 0.f;
    while (t < seg) {
      const float run = std::min(remaining, seg - t);
      const float next_t = t + run;
      const float next_remaining = remaining - run;
      if (inked && run > 0.f && next_t > t)
        draw_line_ex(lerp(a, b, t / seg), lerp(a, b, next_t / seg),
                     thickness, color);
      // A boundary run can fall below the float ulp of t, of remaining, or
      // both (dash 8, gap 5.6 at travelled 68 leaves a 1.9e-6 run): the
      // accumulators round back to themselves and the march loops forever.
      // The pattern boundary is within one ulp, so snapping to it is
      // invisible and guarantees progress.
      if (next_t == t || next_remaining == remaining) {
        remaining = 0.f;
        if (next_t > t)
          t = next_t;
      } else {
        t = next_t;
        remaining = next_remaining;
      }
      if (remaining <= 0.f) {
        inked = !inked;
        remaining = inked ? dash : gap;
      }
    }
  }
}

} // namespace polyline

} // namespace afterhours
