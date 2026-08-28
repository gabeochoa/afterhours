#pragma once

#include <cmath>
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
  float travelled = std::fmod(phase, period);
  if (travelled < 0.f)
    travelled += period;

  for (size_t i = 0; i + 1 < pts.size(); i++) {
    const Vector2Type a = pts[i];
    const Vector2Type b = pts[i + 1];
    const float seg = distance(a, b);
    if (seg <= 0.f)
      continue;

    float t = 0.f;
    while (t < seg) {
      const float in_period = std::fmod(travelled, period);
      const bool inked = in_period < dash;
      const float until_switch =
          inked ? (dash - in_period) : (period - in_period);
      const float run = std::min(until_switch, seg - t);
      if (inked)
        draw_line_ex(lerp(a, b, t / seg), lerp(a, b, (t + run) / seg),
                     thickness, color);
      t += run;
      travelled += run;
    }
  }
}

} // namespace polyline

} // namespace afterhours
