#pragma once

#include "../../drawing_helpers.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>

namespace afterhours::ui {

inline void draw_uniform_border(RectangleType rect, float roundness, int segments,
                                std::bitset<4> corners, float thickness, Color color) {
  const float shorter = std::min(rect.width, rect.height);
  if (shorter <= 0.f || thickness <= 0.f || color.a == 0) return;
  const float width = std::min(thickness, shorter * .5f);
  const float radius = std::clamp(roundness, 0.f, 1.f) * shorter * .5f;
  const int steps = std::max(4, segments);
#ifdef AFTER_HOURS_USE_RAYLIB
  const std::array<int, 4> bits{1, 3, 2, 0};
#else
  const std::array<int, 4> bits{2, 0, 1, 3};
#endif
  Vector2Type first_outer{}, first_inner{}, previous_outer{}, previous_inner{};
  const auto join = [&](Vector2Type outer, Vector2Type inner) {
    draw_triangle(previous_outer, previous_inner, outer, color);
    draw_triangle(outer, previous_inner, inner, color);
  };
  for (int corner = 0; corner < 4; ++corner) {
    const float r = corners.test(bits[corner]) ? radius : 0.f;
    const float inner_radius = std::max(0.f, r - width);
    const bool right = corner < 2;
    const bool bottom = corner == 1 || corner == 2;
    const Vector2Type center{rect.x + (right ? rect.width - r : r),
                             rect.y + (bottom ? rect.height - r : r)};
    const Vector2Type inner_center{
        rect.x + (right ? rect.width - width - inner_radius : width + inner_radius),
        rect.y + (bottom ? rect.height - width - inner_radius : width + inner_radius)};
    const int count = r > 0.f ? steps : 1;
    for (int step = 0; step <= count; ++step) {
      const float angle = (static_cast<float>(corner - 1) +
                           static_cast<float>(step) / count) * 1.57079632679f;
      const float x = std::cos(angle), y = std::sin(angle);
      const Vector2Type outer{center.x + r * x, center.y + r * y};
      const Vector2Type inner{inner_center.x + inner_radius * x,
                              inner_center.y + inner_radius * y};
      if (corner == 0 && step == 0) {
        first_outer = outer;
        first_inner = inner;
      } else {
        join(outer, inner);
      }
      previous_outer = outer;
      previous_inner = inner;
    }
  }
  join(first_outer, first_inner);
}

}
