#pragma once

#include "../../drawing_helpers.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>

namespace afterhours::ui {

enum class BorderEdge { Top, Right, Bottom, Left, All };

inline void draw_uniform_border(RectangleType rect, float roundness, int segments,
                                std::bitset<4> corners, float thickness, Color color,
                                BorderEdge edge = BorderEdge::All) {
  const float shorter = std::min(rect.width, rect.height);
  if (shorter <= 0.f || thickness <= 0.f || color.a == 0) return;
  const float width = std::min(thickness, shorter * .5f);
  const float radius = std::clamp(roundness, 0.f, 1.f) * shorter * .5f;
  const int steps = std::max(4, segments);
  const std::array<int, 4> bits{2, 0, 1, 3};
  const auto draws = [edge](int side) {
    return edge == BorderEdge::All || static_cast<int>(edge) == side;
  };
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
    Vector2Type inner_center{
        rect.x + (right ? rect.width - width - inner_radius : width + inner_radius),
        rect.y + (bottom ? rect.height - width - inner_radius : width + inner_radius)};
    // A lone stroke owns the full arc at either end. At a square end,
    // extend it to the edge instead of leaving half of a miter unpainted.
    if (r == 0.f && edge != BorderEdge::All) {
      if (edge == BorderEdge::Top || edge == BorderEdge::Bottom) inner_center.x = center.x;
      else inner_center.y = center.y;
    }
    const int count = r > 0.f ? steps : 1;
    for (int step = 0; step <= count; ++step) {
      const float angle = (static_cast<float>(corner - 1) +
                           static_cast<float>(step) / count) * PI * 0.5f;
      const float x = std::cos(angle), y = std::sin(angle);
      const Vector2Type outer{center.x + r * x, center.y + r * y};
      const Vector2Type inner{inner_center.x + inner_radius * x,
                              inner_center.y + inner_radius * y};
      if (corner == 0 && step == 0) {
        first_outer = outer;
        first_inner = inner;
      } else if (draws(corner) || (step > 0 && draws((corner + 1) % 4))) {
        join(outer, inner);
      }
      previous_outer = outer;
      previous_inner = inner;
    }
  }
  if (draws(0)) join(first_outer, first_inner);
}

}
