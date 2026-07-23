
#pragma once

#include <bitset>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "../../developer.h"
#include "../../graphics.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "font_helper.h"
#include "image_decode.h"

namespace afterhours {

namespace metal_draw_detail {
inline void set_color(const Color c) { sgl_c4b(c.r, c.g, c.b, c.a); }
} // namespace metal_draw_detail

// Forward declaration: draw_line_ex (below) calls draw_circle_v for round
// end-caps, but draw_circle_v is defined further down. Declare it here so the
// call site compiles regardless of definition order.
inline void draw_circle_v(Vector2Type center, float radius, Color color);

inline void draw_text_ex(const afterhours::Font font, const char *content,
                         const Vector2Type position, const float font_size,
                         const float, const Color color, const float = 0.0f,
                         const float = 0.0f, const float = 0.0f) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx)
    return;
  int fid = (font.id != FONS_INVALID) ? font.id
                                      : graphics::metal_detail::g_active_font;
  if (fid == FONS_INVALID)
    return;
  fonsSetFont(ctx, fid);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  fonsSetColor(ctx, sfons_rgba(color.r, color.g, color.b, color.a));

  // Rasterise glyphs at native DPI for crisp text on Retina displays.
  // We scale the fontstash size up by dpi_scale and draw in a coordinate
  // space that is 1/dpi_scale, so the resulting quads land at the correct
  // logical pixel position.
  float dpi = sapp_dpi_scale();
  if (dpi > 1.01f) {
    float inv = 1.0f / dpi;
    fonsSetSize(ctx, font_size * dpi);
    sgl_push_matrix();
    sgl_scale(inv, inv, 1.0f);
    fonsDrawText(ctx, position.x * dpi, position.y * dpi, content, nullptr);
    sgl_pop_matrix();
  } else {
    fonsSetSize(ctx, font_size);
    fonsDrawText(ctx, position.x, position.y, content, nullptr);
  }
}

inline void draw_text(const char *content, const float x, const float y,
                      const float font_size, const Color color) {
  auto *ctx = graphics::metal_detail::g_fons_ctx;
  if (!ctx || graphics::metal_detail::g_active_font == FONS_INVALID)
    return;
  fonsSetFont(ctx, graphics::metal_detail::g_active_font);
  fonsSetAlign(ctx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
  fonsSetColor(ctx, sfons_rgba(color.r, color.g, color.b, color.a));

  float dpi = sapp_dpi_scale();
  if (dpi > 1.01f) {
    float inv = 1.0f / dpi;
    fonsSetSize(ctx, font_size * dpi);
    sgl_push_matrix();
    sgl_scale(inv, inv, 1.0f);
    fonsDrawText(ctx, x * dpi, y * dpi, content, nullptr);
    sgl_pop_matrix();
  } else {
    fonsSetSize(ctx, font_size);
    fonsDrawText(ctx, x, y, content, nullptr);
  }
}

inline void draw_rectangle(const RectangleType rect, const Color color) {
  sgl_begin_quads();
  metal_draw_detail::set_color(color);
  sgl_v2f(rect.x, rect.y);
  sgl_v2f(rect.x + rect.width, rect.y);
  sgl_v2f(rect.x + rect.width, rect.y + rect.height);
  sgl_v2f(rect.x, rect.y + rect.height);
  sgl_end();
}

inline void draw_rectangle_outline(const RectangleType rect, const Color color,
                                   const float thickness) {
  // Draw 4 thin rects to form an outline
  float x = rect.x, y = rect.y, w = rect.width, h = rect.height, t = thickness;
  draw_rectangle(RectangleType{x, y, w, t}, color);             // top
  draw_rectangle(RectangleType{x, y + h - t, w, t}, color);     // bottom
  draw_rectangle(RectangleType{x, y + t, t, h - 2 * t}, color); // left
  draw_rectangle(RectangleType{x + w - t, y + t, t, h - 2 * t}, color); // right
}

inline void draw_rectangle_outline(const RectangleType rect,
                                   const Color color) {
  draw_rectangle_outline(rect, color, 1.0f);
}

inline void draw_rectangle_rounded(
    const RectangleType rect, const float roundness, const int segments,
    const Color color,
    const std::bitset<4> corners = std::bitset<4>().reset()) {
  if (corners.none() || roundness <= 0.0f) {
    draw_rectangle(rect, color);
    return;
  }

  const float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
  const float shorter = (w < h) ? w : h;
  float r = (shorter * 0.5f) * roundness;
  if (r > w * 0.5f)
    r = w * 0.5f;
  if (r > h * 0.5f)
    r = h * 0.5f;

  const int segs = (segments > 0) ? segments : 8;
  const float PI_HALF = 3.14159265f * 0.5f;

  // Bit layout: 3=TL, 2=TR, 1=BL, 0=BR
  float rTL = corners.test(3) ? r : 0.0f;
  float rTR = corners.test(2) ? r : 0.0f;
  float rBL = corners.test(1) ? r : 0.0f;
  float rBR = corners.test(0) ? r : 0.0f;

  // Center of the rectangle for triangle fan
  float cx = x + w * 0.5f;
  float cy = y + h * 0.5f;

  metal_draw_detail::set_color(color);

  // Helper: emit a corner arc as triangles from center
  auto emit_corner_arc = [&](float cornerX, float cornerY, float radius,
                             float startAngle) {
    if (radius <= 0.0f) {
      // Sharp corner: just one triangle from center to corner point
      sgl_begin_triangles();
      metal_draw_detail::set_color(color);
      sgl_v2f(cx, cy);
      sgl_v2f(cornerX, cornerY);
      // We'll handle connectivity outside
      sgl_end();
      return;
    }
    for (int i = 0; i < segs; i++) {
      float a0 = startAngle +
                 PI_HALF * static_cast<float>(i) / static_cast<float>(segs);
      float a1 = startAngle +
                 PI_HALF * static_cast<float>(i + 1) / static_cast<float>(segs);
      sgl_begin_triangles();
      metal_draw_detail::set_color(color);
      sgl_v2f(cx, cy);
      sgl_v2f(cornerX + radius * cosf(a0), cornerY + radius * sinf(a0));
      sgl_v2f(cornerX + radius * cosf(a1), cornerY + radius * sinf(a1));
      sgl_end();
    }
  };

  // Build rounded rect as triangles from center to perimeter
  // Top-left corner arc (center at x+rTL, y+rTL), angles PI to 1.5*PI
  emit_corner_arc(x + rTL, y + rTL, rTL, 3.14159265f);

  // Top edge: TL arc end -> TR arc start
  sgl_begin_triangles();
  metal_draw_detail::set_color(color);
  sgl_v2f(cx, cy);
  sgl_v2f(x + rTL, y);
  sgl_v2f(x + w - rTR, y);
  sgl_end();

  // Top-right corner arc (center at x+w-rTR, y+rTR), angles -PI/2 to 0
  emit_corner_arc(x + w - rTR, y + rTR, rTR, -PI_HALF);

  // Right edge: TR arc end -> BR arc start
  sgl_begin_triangles();
  metal_draw_detail::set_color(color);
  sgl_v2f(cx, cy);
  sgl_v2f(x + w, y + rTR);
  sgl_v2f(x + w, y + h - rBR);
  sgl_end();

  // Bottom-right corner arc (center at x+w-rBR, y+h-rBR), angles 0 to PI/2
  emit_corner_arc(x + w - rBR, y + h - rBR, rBR, 0.0f);

  // Bottom edge: BR arc end -> BL arc start
  sgl_begin_triangles();
  metal_draw_detail::set_color(color);
  sgl_v2f(cx, cy);
  sgl_v2f(x + w - rBR, y + h);
  sgl_v2f(x + rBL, y + h);
  sgl_end();

  // Bottom-left corner arc (center at x+rBL, y+h-rBL), angles PI/2 to PI
  emit_corner_arc(x + rBL, y + h - rBL, rBL, PI_HALF);

  // Left edge: BL arc end -> TL arc start
  sgl_begin_triangles();
  metal_draw_detail::set_color(color);
  sgl_v2f(cx, cy);
  sgl_v2f(x, y + h - rBL);
  sgl_v2f(x, y + rTL);
  sgl_end();
}

inline void
draw_rectangle_rounded_rotated(const RectangleType rect, const float roundness,
                               const int segments, const Color color,
                               const std::bitset<4> corners,
                               const float rotation) {
  if (std::abs(rotation) >= 0.001f) {
    const float centerX = rect.x + rect.width * 0.5f;
    const float centerY = rect.y + rect.height * 0.5f;
    sgl_push_matrix();
    sgl_translate(centerX, centerY, 0.0f);
    sgl_rotate(sgl_rad(rotation), 0.0f, 0.0f, 1.0f);
    sgl_translate(-centerX, -centerY, 0.0f);
    draw_rectangle_rounded(rect, roundness, segments, color, corners);
    sgl_pop_matrix();
    return;
  }
  draw_rectangle_rounded(rect, roundness, segments, color, corners);
}

inline void draw_rectangle_rounded_lines(
    const RectangleType rect, const float roundness, const int segments,
    const Color color,
    const std::bitset<4> corners = std::bitset<4>().reset()) {
  if (corners.none() || roundness <= 0.0f) {
    draw_rectangle_outline(rect, color);
    return;
  }

  const float x = rect.x, y = rect.y, w = rect.width, h = rect.height;
  const float shorter = (w < h) ? w : h;
  float r = (shorter * 0.5f) * roundness;
  if (r > w * 0.5f)
    r = w * 0.5f;
  if (r > h * 0.5f)
    r = h * 0.5f;

  const int segs = (segments > 0) ? segments : 8;
  const float PI_HALF = 3.14159265f * 0.5f;

  float rTL = corners.test(3) ? r : 0.0f;
  float rTR = corners.test(2) ? r : 0.0f;
  float rBL = corners.test(1) ? r : 0.0f;
  float rBR = corners.test(0) ? r : 0.0f;

  sgl_begin_line_strip();
  metal_draw_detail::set_color(color);

  // Top edge (left to right)
  sgl_v2f(x + rTL, y);
  sgl_v2f(x + w - rTR, y);

  // Top-right arc
  for (int i = 0; i <= segs; i++) {
    float a =
        -PI_HALF + PI_HALF * static_cast<float>(i) / static_cast<float>(segs);
    sgl_v2f(x + w - rTR + rTR * cosf(a), y + rTR + rTR * sinf(a));
  }

  // Right edge
  sgl_v2f(x + w, y + h - rBR);

  // Bottom-right arc
  for (int i = 0; i <= segs; i++) {
    float a = PI_HALF * static_cast<float>(i) / static_cast<float>(segs);
    sgl_v2f(x + w - rBR + rBR * cosf(a), y + h - rBR + rBR * sinf(a));
  }

  // Bottom edge (right to left)
  sgl_v2f(x + rBL, y + h);

  // Bottom-left arc
  for (int i = 0; i <= segs; i++) {
    float a =
        PI_HALF + PI_HALF * static_cast<float>(i) / static_cast<float>(segs);
    sgl_v2f(x + rBL + rBL * cosf(a), y + h - rBL + rBL * sinf(a));
  }

  // Left edge
  sgl_v2f(x, y + rTL);

  // Top-left arc
  for (int i = 0; i <= segs; i++) {
    float a = 3.14159265f +
              PI_HALF * static_cast<float>(i) / static_cast<float>(segs);
    sgl_v2f(x + rTL + rTL * cosf(a), y + rTL + rTL * sinf(a));
  }

  sgl_end();
}

inline void draw_rectangle_rounded_lines_ex(const RectangleType rect,
                                            const float roundness,
                                            const int segments,
                                            const float thickness,
                                            const Color color) {
  if (thickness <= 1.0f) {
    draw_rectangle_rounded_lines(rect, roundness, segments, color,
                                 std::bitset<4>().set());
    return;
  }
  const int layers = std::max(1, static_cast<int>(std::round(thickness)));
  for (int i = 0; i < layers; ++i) {
    const float inset = static_cast<float>(i);
    const RectangleType r{rect.x + inset, rect.y + inset,
                          std::max(0.0f, rect.width - 2.0f * inset),
                          std::max(0.0f, rect.height - 2.0f * inset)};
    if (r.width <= 0.0f || r.height <= 0.0f)
      break;
    draw_rectangle_rounded_lines(r, roundness, segments, color,
                                 std::bitset<4>().set());
  }
}

inline void draw_rectangle_gradient_v(const RectangleType rect, const Color top,
                                      const Color bottom) {
  sgl_begin_quads();
  sgl_c4b(top.r, top.g, top.b, top.a);
  sgl_v2f(rect.x, rect.y);
  sgl_v2f(rect.x + rect.width, rect.y);
  sgl_c4b(bottom.r, bottom.g, bottom.b, bottom.a);
  sgl_v2f(rect.x + rect.width, rect.y + rect.height);
  sgl_v2f(rect.x, rect.y + rect.height);
  sgl_end();
}

inline void draw_rectangle_gradient_h(const RectangleType rect,
                                      const Color left, const Color right) {
  sgl_begin_quads();
  sgl_c4b(left.r, left.g, left.b, left.a);
  sgl_v2f(rect.x, rect.y);
  sgl_v2f(rect.x, rect.y + rect.height);
  sgl_c4b(right.r, right.g, right.b, right.a);
  sgl_v2f(rect.x + rect.width, rect.y + rect.height);
  sgl_v2f(rect.x + rect.width, rect.y);
  sgl_end();
}

inline void draw_rectangle_gradient_ex(const RectangleType rect, const Color tl,
                                       const Color bl, const Color tr,
                                       const Color br) {
  sgl_begin_quads();
  sgl_c4b(tl.r, tl.g, tl.b, tl.a);
  sgl_v2f(rect.x, rect.y);
  sgl_c4b(bl.r, bl.g, bl.b, bl.a);
  sgl_v2f(rect.x, rect.y + rect.height);
  sgl_c4b(br.r, br.g, br.b, br.a);
  sgl_v2f(rect.x + rect.width, rect.y + rect.height);
  sgl_c4b(tr.r, tr.g, tr.b, tr.a);
  sgl_v2f(rect.x + rect.width, rect.y);
  sgl_end();
}

inline void draw_circle_gradient(int centerX, int centerY, float radius,
                                 Color inner, Color outer) {
  constexpr int SEGMENTS = 40;
  const float cx = static_cast<float>(centerX);
  const float cy = static_cast<float>(centerY);
  sgl_begin_triangles();
  for (int i = 0; i < SEGMENTS; ++i) {
    const float a0 =
        static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(SEGMENTS);
    const float a1 = static_cast<float>(i + 1) * 2.0f * 3.14159265f /
                     static_cast<float>(SEGMENTS);
    sgl_c4b(inner.r, inner.g, inner.b, inner.a);
    sgl_v2f(cx, cy);
    sgl_c4b(outer.r, outer.g, outer.b, outer.a);
    sgl_v2f(cx + radius * std::cos(a0), cy + radius * std::sin(a0));
    sgl_v2f(cx + radius * std::cos(a1), cy + radius * std::sin(a1));
  }
  sgl_end();
}

inline void draw_texture_npatch(const texture_manager::Texture,
                                const RectangleType, int, int, int, int,
                                const Color) {
  log_error("@notimplemented draw_texture_npatch");
}

// Draw a ring segment (annular sector) between innerRadius and outerRadius,
// sweeping from startAngle to endAngle (degrees). Matches raylib's DrawRing:
// filled with a quad strip between the two radii.
inline void draw_ring_segment(float centerX, float centerY, float innerRadius,
                              float outerRadius, float startAngle,
                              float endAngle, int segments, Color color) {
  if (startAngle == endAngle)
    return;

  // Ensure outer >= inner (raylib swaps if reversed).
  if (outerRadius < innerRadius) {
    float tmp = outerRadius;
    outerRadius = innerRadius;
    innerRadius = tmp;
  }
  if (outerRadius <= 0.0f)
    outerRadius = 0.1f;

  // Normalise so we always sweep from the smaller to the larger angle.
  if (endAngle < startAngle) {
    float tmp = startAngle;
    startAngle = endAngle;
    endAngle = tmp;
  }

  if (segments < 4) {
    // Estimate segments from a smooth-error heuristic (matches raylib).
    float th =
        acosf(2.0f * powf(1.0f - 0.5f / outerRadius, 2.0f) - 1.0f);
    segments =
        static_cast<int>((endAngle - startAngle) / (th * RAD2DEG) / 2.0f);
    if (segments <= 0)
      segments = 4;
  }

  // Special case: a degenerate inner radius makes this a circle sector.
  const float stepLength =
      (endAngle - startAngle) / static_cast<float>(segments);
  float angle = startAngle;

  metal_draw_detail::set_color(color);
  sgl_begin_triangles();
  for (int i = 0; i < segments; i++) {
    const float a0 = DEG2RAD * angle;
    const float a1 = DEG2RAD * (angle + stepLength);

    const float outerX0 = centerX + cosf(a0) * outerRadius;
    const float outerY0 = centerY + sinf(a0) * outerRadius;
    const float outerX1 = centerX + cosf(a1) * outerRadius;
    const float outerY1 = centerY + sinf(a1) * outerRadius;
    const float innerX0 = centerX + cosf(a0) * innerRadius;
    const float innerY0 = centerY + sinf(a0) * innerRadius;
    const float innerX1 = centerX + cosf(a1) * innerRadius;
    const float innerY1 = centerY + sinf(a1) * innerRadius;

    // Two triangles form the quad between the inner and outer arc for this
    // segment (winding matches raylib's DrawRing).
    sgl_v2f(outerX0, outerY0);
    sgl_v2f(innerX0, innerY0);
    sgl_v2f(innerX1, innerY1);

    sgl_v2f(outerX0, outerY0);
    sgl_v2f(innerX1, innerY1);
    sgl_v2f(outerX1, outerY1);

    angle += stepLength;
  }
  sgl_end();
}

// Draw a full ring (circle with a hole) — a ring segment sweeping 0..360.
inline void draw_ring(float centerX, float centerY, float innerRadius,
                      float outerRadius, int segments, Color color) {
  draw_ring_segment(centerX, centerY, innerRadius, outerRadius, 0.0f, 360.0f,
                    segments, color);
}

inline void begin_scissor_mode(int x, int y, int w, int h) {
  // Scissor operates in framebuffer pixels; scale logical coords by DPI.
  float dpi = sapp_dpi_scale();
  sgl_scissor_rect(
      static_cast<int>(static_cast<float>(x) * dpi),
      static_cast<int>(static_cast<float>(y) * dpi),
      static_cast<int>(static_cast<float>(w) * dpi),
      static_cast<int>(static_cast<float>(h) * dpi),
      true);
}

inline void end_scissor_mode() {
  // Reset scissor to full viewport (framebuffer pixels)
  sgl_scissor_rect(0, 0, sapp_width(), sapp_height(), true);
}

inline void push_rotation(float centerX, float centerY, float rotation) {
  sgl_push_matrix();
  if (std::abs(rotation) >= 0.001f) {
    sgl_translate(centerX, centerY, 0.0f);
    sgl_rotate(sgl_rad(rotation), 0.0f, 0.0f, 1.0f);
    sgl_translate(-centerX, -centerY, 0.0f);
  }
}

inline void pop_rotation() { sgl_pop_matrix(); }

inline void draw_line(int x1, int y1, int x2, int y2, Color color) {
  sgl_begin_lines();
  metal_draw_detail::set_color(color);
  sgl_v2f(static_cast<float>(x1), static_cast<float>(y1));
  sgl_v2f(static_cast<float>(x2), static_cast<float>(y2));
  sgl_end();
}

inline void draw_line_ex(Vector2Type start, Vector2Type end, float thickness,
                         Color color) {
  const float dx = end.x - start.x;
  const float dy = end.y - start.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0.0001f) {
    draw_circle_v(start, std::max(0.5f, thickness * 0.5f), color);
    return;
  }

  if (thickness <= 1.0f) {
    sgl_begin_lines();
    metal_draw_detail::set_color(color);
    sgl_v2f(start.x, start.y);
    sgl_v2f(end.x, end.y);
    sgl_end();
    return;
  }

  const float half = thickness * 0.5f;
  const float nx = -dy / len;
  const float ny = dx / len;
  const float ox = nx * half;
  const float oy = ny * half;

  const float ax = start.x + ox;
  const float ay = start.y + oy;
  const float bx = end.x + ox;
  const float by = end.y + oy;
  const float cx = end.x - ox;
  const float cy = end.y - oy;
  const float dx2 = start.x - ox;
  const float dy2 = start.y - oy;

  sgl_begin_quads();
  metal_draw_detail::set_color(color);
  sgl_v2f(ax, ay);
  sgl_v2f(bx, by);
  sgl_v2f(cx, cy);
  sgl_v2f(dx2, dy2);
  sgl_end();
}

inline void draw_line_strip(Vector2Type *points, int point_count, Color color) {
  if (point_count < 2)
    return;
  sgl_begin_line_strip();
  metal_draw_detail::set_color(color);
  for (int i = 0; i < point_count; i++) {
    sgl_v2f(points[i].x, points[i].y);
  }
  sgl_end();
}

inline bool check_collision_point_line(Vector2Type point, Vector2Type p1,
                                       Vector2Type p2, float threshold) {
  const float vx = p2.x - p1.x;
  const float vy = p2.y - p1.y;
  const float len_sq = vx * vx + vy * vy;
  if (len_sq <= 1e-8f) {
    const float dx = point.x - p1.x;
    const float dy = point.y - p1.y;
    return (dx * dx + dy * dy) <= (threshold * threshold);
  }

  float t = ((point.x - p1.x) * vx + (point.y - p1.y) * vy) / len_sq;
  if (t < 0.0f)
    t = 0.0f;
  if (t > 1.0f)
    t = 1.0f;

  const float proj_x = p1.x + t * vx;
  const float proj_y = p1.y + t * vy;
  const float dx = point.x - proj_x;
  const float dy = point.y - proj_y;
  return (dx * dx + dy * dy) <= (threshold * threshold);
}

inline bool check_collision_recs(RectangleType a, RectangleType b) {
  return (a.x < b.x + b.width) && (a.x + a.width > b.x) &&
         (a.y < b.y + b.height) && (a.y + a.height > b.y);
}

inline RectangleType get_collision_rec(RectangleType a, RectangleType b) {
  if (!check_collision_recs(a, b))
    return RectangleType{0, 0, 0, 0};
  const float x1 = std::max(a.x, b.x);
  const float y1 = std::max(a.y, b.y);
  const float x2 = std::min(a.x + a.width, b.x + b.width);
  const float y2 = std::min(a.y + a.height, b.y + b.height);
  return RectangleType{x1, y1, x2 - x1, y2 - y1};
}

inline void draw_circle(int centerX, int centerY, float radius, Color color) {
  constexpr int SEGMENTS = 32;
  sgl_begin_triangle_strip();
  metal_draw_detail::set_color(color);
  float cx = static_cast<float>(centerX);
  float cy = static_cast<float>(centerY);
  for (int i = 0; i <= SEGMENTS; i++) {
    float angle = static_cast<float>(i) * 2.0f * 3.14159265f /
                  static_cast<float>(SEGMENTS);
    sgl_v2f(cx, cy);
    sgl_v2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
  }
  sgl_end();
}

inline void draw_circle_v(Vector2Type center, float radius, Color color) {
  draw_circle(static_cast<int>(center.x), static_cast<int>(center.y), radius,
              color);
}

inline void draw_circle_lines(int centerX, int centerY, float radius,
                              Color color) {
  constexpr int SEGMENTS = 32;
  sgl_begin_line_strip();
  metal_draw_detail::set_color(color);
  float cx = static_cast<float>(centerX);
  float cy = static_cast<float>(centerY);
  for (int i = 0; i <= SEGMENTS; i++) {
    float angle = static_cast<float>(i) * 2.0f * 3.14159265f /
                  static_cast<float>(SEGMENTS);
    sgl_v2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
  }
  sgl_end();
}

// Draw a filled circle sector (pie slice) from startAngle to endAngle
// (degrees), as a triangle fan from the center. Matches raylib.
inline void draw_circle_sector(Vector2Type center, float radius,
                               float startAngle, float endAngle, int segments,
                               Color color) {
  if (radius <= 0.0f)
    radius = 0.1f;

  // Sweep from smaller to larger angle.
  if (endAngle < startAngle) {
    float tmp = startAngle;
    startAngle = endAngle;
    endAngle = tmp;
  }

  const int minSegments =
      static_cast<int>(ceilf((endAngle - startAngle) / 90.0f));
  if (segments < minSegments) {
    float th = acosf(2.0f * powf(1.0f - 0.5f / radius, 2.0f) - 1.0f);
    segments =
        static_cast<int>((endAngle - startAngle) / (th * RAD2DEG) / 2.0f);
    if (segments <= 0)
      segments = minSegments;
  }

  const float stepLength =
      (endAngle - startAngle) / static_cast<float>(segments);
  float angle = startAngle;

  metal_draw_detail::set_color(color);
  sgl_begin_triangles();
  for (int i = 0; i < segments; i++) {
    const float a0 = DEG2RAD * angle;
    const float a1 = DEG2RAD * (angle + stepLength);
    sgl_v2f(center.x, center.y);
    sgl_v2f(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
    sgl_v2f(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
    angle += stepLength;
  }
  sgl_end();
}
// Draw the outline of a circle sector: the two radii plus the arc.
inline void draw_circle_sector_lines(Vector2Type center, float radius,
                                     float startAngle, float endAngle,
                                     int segments, Color color) {
  if (radius <= 0.0f)
    radius = 0.1f;

  if (endAngle < startAngle) {
    float tmp = startAngle;
    startAngle = endAngle;
    endAngle = tmp;
  }

  const int minSegments =
      static_cast<int>(ceilf((endAngle - startAngle) / 90.0f));
  if (segments < minSegments) {
    float th = acosf(2.0f * powf(1.0f - 0.5f / radius, 2.0f) - 1.0f);
    segments =
        static_cast<int>((endAngle - startAngle) / (th * RAD2DEG) / 2.0f);
    if (segments <= 0)
      segments = minSegments;
  }

  // Draw the cap lines to the center only when the sector is not a full
  // circle (matches raylib's showCapLines behaviour).
  const bool showCapLines = (endAngle - startAngle) < 360.0f;
  const float stepLength =
      (endAngle - startAngle) / static_cast<float>(segments);
  float angle = startAngle;

  metal_draw_detail::set_color(color);
  sgl_begin_line_strip();
  if (showCapLines)
    sgl_v2f(center.x, center.y);
  for (int i = 0; i <= segments; i++) {
    const float a = DEG2RAD * angle;
    sgl_v2f(center.x + cosf(a) * radius, center.y + sinf(a) * radius);
    angle += stepLength;
  }
  if (showCapLines)
    sgl_v2f(center.x, center.y);
  sgl_end();
}
// Draw a filled ellipse as a triangle fan (a circle scaled per-axis).
inline void draw_ellipse(int centerX, int centerY, float radiusH,
                         float radiusV, Color color) {
  constexpr int SEGMENTS = 36;
  const float cx = static_cast<float>(centerX);
  const float cy = static_cast<float>(centerY);
  metal_draw_detail::set_color(color);
  sgl_begin_triangles();
  for (int i = 0; i < SEGMENTS; i++) {
    const float a0 = static_cast<float>(i) * 2.0f * 3.14159265f /
                     static_cast<float>(SEGMENTS);
    const float a1 = static_cast<float>(i + 1) * 2.0f * 3.14159265f /
                     static_cast<float>(SEGMENTS);
    sgl_v2f(cx, cy);
    sgl_v2f(cx + cosf(a1) * radiusH, cy + sinf(a1) * radiusV);
    sgl_v2f(cx + cosf(a0) * radiusH, cy + sinf(a0) * radiusV);
  }
  sgl_end();
}
// Draw the outline of an ellipse as a closed line loop.
inline void draw_ellipse_lines(int centerX, int centerY, float radiusH,
                               float radiusV, Color color) {
  constexpr int SEGMENTS = 36;
  const float cx = static_cast<float>(centerX);
  const float cy = static_cast<float>(centerY);
  metal_draw_detail::set_color(color);
  sgl_begin_line_strip();
  for (int i = 0; i <= SEGMENTS; i++) {
    const float a = static_cast<float>(i) * 2.0f * 3.14159265f /
                    static_cast<float>(SEGMENTS);
    sgl_v2f(cx + cosf(a) * radiusH, cy + sinf(a) * radiusV);
  }
  sgl_end();
}

inline void draw_triangle(Vector2Type v1, Vector2Type v2, Vector2Type v3,
                          Color color) {
  sgl_begin_triangles();
  metal_draw_detail::set_color(color);
  sgl_v2f(v1.x, v1.y);
  sgl_v2f(v2.x, v2.y);
  sgl_v2f(v3.x, v3.y);
  sgl_end();
}

inline void draw_triangle_lines(Vector2Type v1, Vector2Type v2, Vector2Type v3,
                                Color color) {
  sgl_begin_line_strip();
  metal_draw_detail::set_color(color);
  sgl_v2f(v1.x, v1.y);
  sgl_v2f(v2.x, v2.y);
  sgl_v2f(v3.x, v3.y);
  sgl_v2f(v1.x, v1.y);
  sgl_end();
}

// Draw a filled regular polygon (triangle fan from center). rotation in
// degrees. Matches raylib's DrawPoly.
inline void draw_poly(Vector2Type center, int sides, float radius,
                      float rotation, Color color) {
  if (sides < 3)
    sides = 3;

  const float centralAngleStep = 360.0f / static_cast<float>(sides);
  float centralAngle = rotation;

  metal_draw_detail::set_color(color);
  sgl_begin_triangles();
  for (int i = 0; i < sides; i++) {
    const float a0 = DEG2RAD * centralAngle;
    const float a1 = DEG2RAD * (centralAngle + centralAngleStep);
    sgl_v2f(center.x, center.y);
    sgl_v2f(center.x + cosf(a1) * radius, center.y + sinf(a1) * radius);
    sgl_v2f(center.x + cosf(a0) * radius, center.y + sinf(a0) * radius);
    centralAngle += centralAngleStep;
  }
  sgl_end();
}
// Draw the outline of a regular polygon as a closed line loop.
inline void draw_poly_lines(Vector2Type center, int sides, float radius,
                            float rotation, Color color) {
  if (sides < 3)
    sides = 3;

  const float centralAngleStep = 360.0f / static_cast<float>(sides);
  float centralAngle = rotation;

  metal_draw_detail::set_color(color);
  sgl_begin_line_strip();
  for (int i = 0; i <= sides; i++) {
    const float a = DEG2RAD * centralAngle;
    sgl_v2f(center.x + cosf(a) * radius, center.y + sinf(a) * radius);
    centralAngle += centralAngleStep;
  }
  sgl_end();
}
// Draw a regular polygon outline with a given thickness, built from a quad
// per edge (inner/outer offset). Matches raylib's DrawPolyLinesEx.
inline void draw_poly_lines_ex(Vector2Type center, int sides, float radius,
                               float rotation, float lineThick, Color color) {
  if (sides < 3)
    sides = 3;

  const float centralAngleStep = 360.0f / static_cast<float>(sides);
  const float exteriorAngle = 360.0f / static_cast<float>(sides) * DEG2RAD;
  const float innerRadius =
      radius - lineThick * cosf(exteriorAngle / 2.0f);
  float centralAngle = rotation;

  metal_draw_detail::set_color(color);
  sgl_begin_triangles();
  for (int i = 0; i < sides; i++) {
    const float a0 = DEG2RAD * centralAngle;
    const float a1 = DEG2RAD * (centralAngle + centralAngleStep);

    const float outerX0 = center.x + cosf(a0) * radius;
    const float outerY0 = center.y + sinf(a0) * radius;
    const float outerX1 = center.x + cosf(a1) * radius;
    const float outerY1 = center.y + sinf(a1) * radius;
    const float innerX0 = center.x + cosf(a0) * innerRadius;
    const float innerY0 = center.y + sinf(a0) * innerRadius;
    const float innerX1 = center.x + cosf(a1) * innerRadius;
    const float innerY1 = center.y + sinf(a1) * innerRadius;

    // Two triangles form the thick edge quad between this vertex and the
    // next (winding matches raylib's DrawPolyLinesEx).
    sgl_v2f(outerX0, outerY0);
    sgl_v2f(innerX1, innerY1);
    sgl_v2f(innerX0, innerY0);

    sgl_v2f(outerX0, outerY0);
    sgl_v2f(outerX1, outerY1);
    sgl_v2f(innerX1, innerY1);

    centralAngle += centralAngleStep;
  }
  sgl_end();
}

inline void set_mouse_cursor(int cursor_id) {
  sapp_set_mouse_cursor(static_cast<sapp_mouse_cursor>(cursor_id));
}

inline afterhours::Font get_default_font() { return afterhours::Font(); }
inline afterhours::Font get_unset_font() { return afterhours::Font(); }

inline graphics::RenderTextureType load_render_texture(int w, int h) {
  graphics::RenderTextureType rt;
  rt.width = w;
  rt.height = h;

  // Query swapchain defaults so render texture formats/sample counts match.
  const auto &env = sg_query_desc().environment.defaults;
  sg_pixel_format color_fmt = env.color_format;
  if (color_fmt == _SG_PIXELFORMAT_DEFAULT)
    color_fmt = SG_PIXELFORMAT_BGRA8;
  sg_pixel_format depth_fmt = env.depth_format;
  if (depth_fmt == _SG_PIXELFORMAT_DEFAULT)
    depth_fmt = SG_PIXELFORMAT_DEPTH_STENCIL;
  int sc = env.sample_count;
  if (sc <= 0)
    sc = 1;

  sg_image_desc cd{};
  cd.usage.color_attachment = true;
  cd.usage.immutable = true;
  cd.width = w;
  cd.height = h;
  cd.pixel_format = color_fmt;
  cd.sample_count = sc;
  cd.label = "rt-color";
  sg_image color = sg_make_image(&cd);

  sg_image_desc dd{};
  dd.usage.depth_stencil_attachment = true;
  dd.usage.immutable = true;
  dd.width = w;
  dd.height = h;
  dd.pixel_format = depth_fmt;
  dd.sample_count = sc;
  dd.label = "rt-depth";
  sg_image depth = sg_make_image(&dd);

  sg_view_desc cvd{};
  cvd.color_attachment.image = color;
  cvd.label = "rt-color-view";
  sg_view color_view = sg_make_view(&cvd);

  sg_view_desc dvd{};
  dvd.depth_stencil_attachment.image = depth;
  dvd.label = "rt-depth-view";
  sg_view depth_view = sg_make_view(&dvd);

  sg_view_desc tvd{};
  tvd.texture.image = color;
  tvd.label = "rt-tex-view";
  sg_view tex_view = sg_make_view(&tvd);

  sg_sampler_desc sd{};
  sd.min_filter = SG_FILTER_LINEAR;
  sd.mag_filter = SG_FILTER_LINEAR;
  sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  sd.label = "rt-sampler";
  sg_sampler smp = sg_make_sampler(&sd);

  // Create an sgl context matching the render texture's format so that
  // sgl_draw() uses a pipeline compatible with the offscreen pass.
  sgl_context_desc_t ctx_desc{};
  ctx_desc.color_format = color_fmt;
  ctx_desc.depth_format = depth_fmt;
  ctx_desc.sample_count = sc;
  sgl_context sgl_ctx = sgl_make_context(&ctx_desc);

  rt.color_img_id = color.id;
  rt.depth_img_id = depth.id;
  rt.color_view_id = color_view.id;
  rt.depth_view_id = depth_view.id;
  rt.tex_view_id = tex_view.id;
  rt.sampler_id = smp.id;
  rt.sgl_ctx_id = sgl_ctx.id;
  return rt;
}

inline void unload_render_texture(graphics::RenderTextureType &rt) {
  if (rt.sgl_ctx_id)
    sgl_destroy_context({rt.sgl_ctx_id});
  if (rt.tex_view_id)
    sg_destroy_view({rt.tex_view_id});
  if (rt.depth_view_id)
    sg_destroy_view({rt.depth_view_id});
  if (rt.color_view_id)
    sg_destroy_view({rt.color_view_id});
  if (rt.depth_img_id)
    sg_destroy_image({rt.depth_img_id});
  if (rt.color_img_id)
    sg_destroy_image({rt.color_img_id});
  if (rt.sampler_id)
    sg_destroy_sampler({rt.sampler_id});
  rt = {};
}

inline void begin_texture_mode(graphics::RenderTextureType &rt) {
  auto unwind_camera_stack = []() {
    if (graphics::metal_detail::g_camera_mode_depth <= 0)
      return;
    while (graphics::metal_detail::g_camera_mode_depth > 0) {
      sgl_matrix_mode_modelview();
      sgl_pop_matrix();
      --graphics::metal_detail::g_camera_mode_depth;
    }
    log_warn("begin_texture_mode: cleared unbalanced begin_mode_2d stack");
  };

  // If this render texture's pass is already active, continue drawing
  // into the existing pass (matches Raylib's begin_texture_mode behavior).
  if (graphics::metal_detail::g_pass_active &&
      graphics::metal_detail::g_active_rt_color_view_id == rt.color_view_id) {
    return;
  }

  // Flush and end the current pass (swapchain or another offscreen)
  if (graphics::metal_detail::g_pass_active) {
    unwind_camera_stack();
    if (graphics::metal_detail::g_fons_ctx)
      sfons_flush(graphics::metal_detail::g_fons_ctx);
    sgl_context_draw(sgl_get_context());
    sg_end_pass();
    graphics::metal_detail::g_pass_active = false;
  }

  // Switch to the render texture's sgl context (matching pixel format)
  sgl_set_context({rt.sgl_ctx_id});

  sg_pass pass{};
  pass.action.colors[0].load_action = SG_LOADACTION_LOAD;
  pass.action.depth.load_action = SG_LOADACTION_CLEAR;
  pass.action.depth.clear_value = 1.0f;
  pass.attachments.colors[0] = {rt.color_view_id};
  pass.attachments.depth_stencil = {rt.depth_view_id};
  sg_begin_pass(&pass);
  graphics::metal_detail::g_pass_active = true;
  graphics::metal_detail::g_active_rt_color_view_id = rt.color_view_id;

  sgl_defaults();
  sgl_matrix_mode_projection();
  // Apply GL→Metal clip-space Z fixup before the ortho projection.
  // clang-format off
  static const float gl_to_metal[16] = {
      1,0,0,0,  0,1,0,0,  0,0,0.5f,0,  0,0,0.5f,1
  };
  // clang-format on
  sgl_load_matrix(gl_to_metal);
  sgl_ortho(0.0f, static_cast<float>(rt.width),
            static_cast<float>(rt.height), 0.0f, -1.0f, 1.0f);

  graphics::metal_detail::g_in_texture_mode = true;
}

inline void end_texture_mode() {
  auto unwind_camera_stack = []() {
    if (graphics::metal_detail::g_camera_mode_depth <= 0)
      return;
    while (graphics::metal_detail::g_camera_mode_depth > 0) {
      sgl_matrix_mode_modelview();
      sgl_pop_matrix();
      --graphics::metal_detail::g_camera_mode_depth;
    }
    log_warn("end_texture_mode: cleared unbalanced begin_mode_2d stack");
  };

  if (graphics::metal_detail::g_pass_active) {
    unwind_camera_stack();
    if (graphics::metal_detail::g_fons_ctx)
      sfons_flush(graphics::metal_detail::g_fons_ctx);
    sgl_context_draw(sgl_get_context());
    sg_end_pass();
    graphics::metal_detail::g_pass_active = false;
    graphics::metal_detail::g_active_rt_color_view_id = 0;
  }

  // Restore the default sgl context (swapchain format).
  // The caller is responsible for starting the next pass
  // (via begin_drawing or another begin_texture_mode).
  sgl_set_context(sgl_default_context());
  graphics::metal_detail::g_in_texture_mode = false;
}

inline void draw_render_texture(const graphics::RenderTextureType &rt, float x,
                                float y, Color tint) {
  sg_view tv = {rt.tex_view_id};
  sg_sampler smp = {rt.sampler_id};
  float w = static_cast<float>(rt.width);
  float h = static_cast<float>(rt.height);

  sgl_enable_texture();
  sgl_texture(tv, smp);
  sgl_begin_quads();
  sgl_c4b(tint.r, tint.g, tint.b, tint.a);
  sgl_v2f_t2f(x, y, 0.0f, 0.0f);
  sgl_v2f_t2f(x + w, y, 1.0f, 0.0f);
  sgl_v2f_t2f(x + w, y + h, 1.0f, 1.0f);
  sgl_v2f_t2f(x, y + h, 0.0f, 1.0f);
  sgl_end();
  sgl_disable_texture();

  if (graphics::metal_detail::g_shader_runtime.ps1_enabled) {
    const float scan_h = static_cast<float>(rt.height);
    const float scan_w = static_cast<float>(rt.width);

    // Approximate CRT scanlines from the ps1 post-process shader.
    sgl_begin_lines();
    sgl_c4b(0, 0, 0, 24);
    for (int yy = 1; yy < rt.height; yy += 2) {
      const float py = y + static_cast<float>(yy);
      sgl_v2f(x, py);
      sgl_v2f(x + scan_w, py);
    }
    sgl_end();

    // Subtle vignette approximation: darken the edges with translucent bands.
    const float band = std::max(8.0f, std::min(scan_w, scan_h) * 0.08f);
    sgl_begin_quads();
    sgl_c4b(0, 0, 0, 26);
    // top
    sgl_v2f(x, y);
    sgl_v2f(x + scan_w, y);
    sgl_v2f(x + scan_w, y + band);
    sgl_v2f(x, y + band);
    // bottom
    sgl_v2f(x, y + scan_h - band);
    sgl_v2f(x + scan_w, y + scan_h - band);
    sgl_v2f(x + scan_w, y + scan_h);
    sgl_v2f(x, y + scan_h);
    // left
    sgl_v2f(x, y + band);
    sgl_v2f(x + band, y + band);
    sgl_v2f(x + band, y + scan_h - band);
    sgl_v2f(x, y + scan_h - band);
    // right
    sgl_v2f(x + scan_w - band, y + band);
    sgl_v2f(x + scan_w, y + band);
    sgl_v2f(x + scan_w, y + scan_h - band);
    sgl_v2f(x + scan_w - band, y + scan_h - band);
    sgl_end();
  }
}

inline void draw_texture_rec(TextureType, RectangleType, Vector2Type, Color) {
  log_warn("draw_texture_rec: not yet implemented in sokol backend");
}

inline void draw_texture_pro(TextureType, RectangleType, RectangleType,
                             Vector2Type, float, Color) {
  log_error("@notimplemented draw_texture_pro");
}

extern "C" bool metal_capture_render_texture(uint32_t color_img_id,
                                             int width, int height,
                                             const char *path);
extern "C" int metal_capture_render_texture_to_memory(
    uint32_t color_img_id, int width, int height,
    uint8_t **out_data, int *out_size);

inline bool capture_render_texture(const graphics::RenderTextureType &rt,
                                   const std::filesystem::path &path) {
  if (rt.color_img_id == 0)
    return false;
  return metal_capture_render_texture(rt.color_img_id, rt.width, rt.height,
                                      path.c_str());
}

inline std::vector<uint8_t>
capture_render_texture_to_memory(const graphics::RenderTextureType &rt) {
  if (rt.color_img_id == 0)
    return {};
  uint8_t *data = nullptr;
  int size = 0;
  if (!metal_capture_render_texture_to_memory(rt.color_img_id, rt.width,
                                              rt.height, &data, &size)) {
    return {};
  }
  std::vector<uint8_t> result(data, data + size);
  free(data);
  return result;
}

inline std::vector<uint8_t> capture_screen_to_memory() {
  log_warn("capture_screen_to_memory: use capture_render_texture_to_memory "
           "with an offscreen render texture instead");
  return {};
}

inline constexpr int TEXTURE_FILTER_BILINEAR = 1;
inline constexpr int TEXTURE_FILTER_TRILINEAR = 2;

namespace metal_texture_detail {

// Map the afterhours filter constant to a sokol sampler filter. Both BILINEAR
// and TRILINEAR use linear min/mag; TRILINEAR additionally uses a linear mipmap
// filter (only meaningful when the image has mip levels).
inline sg_sampler make_sampler_for_filter(int filter) {
  sg_sampler_desc sd{};
  sd.min_filter = SG_FILTER_LINEAR;
  sd.mag_filter = SG_FILTER_LINEAR;
  sd.mipmap_filter =
      (filter == TEXTURE_FILTER_TRILINEAR) ? SG_FILTER_LINEAR : SG_FILTER_NEAREST;
  sd.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
  sd.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
  sd.label = "tex-sampler";
  return sg_make_sampler(&sd);
}

// Upload a tightly-packed RGBA8 pixel buffer to a new immutable GPU image and
// build the matching view + sampler. Shared by load_texture and
// load_texture_with_color_key. Returns an empty TextureType on failure.
inline TextureType load_texture_from_pixels(const unsigned char *rgba, int w,
                                            int h,
                                            int filter = TEXTURE_FILTER_BILINEAR) {
  if (rgba == nullptr || w <= 0 || h <= 0) {
    log_error("load_texture_from_pixels: invalid pixel data ({}x{})", w, h);
    return TextureType{};
  }

  sg_image_desc id{};
  id.usage.immutable = true;
  id.width = w;
  id.height = h;
  id.pixel_format = SG_PIXELFORMAT_RGBA8;
  id.data.mip_levels[0].ptr = rgba;
  id.data.mip_levels[0].size =
      static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  id.label = "tex-image";
  sg_image img = sg_make_image(&id);

  sg_view_desc vd{};
  vd.texture.image = img;
  vd.label = "tex-view";
  sg_view view = sg_make_view(&vd);

  sg_sampler smp = make_sampler_for_filter(filter);

  TextureType tex{};
  tex.width = static_cast<float>(w);
  tex.height = static_cast<float>(h);
  tex.img_id = img.id;
  tex.view_id = view.id;
  tex.sampler_id = smp.id;
  return tex;
}

} // namespace metal_texture_detail

inline TextureType load_texture(const char *path) {
  int w = 0, h = 0, comp = 0;
  // Force 4 channels (RGBA8) to match the fixed pixel format below.
  unsigned char *pixels = stbi_load(path, &w, &h, &comp, 4);
  if (pixels == nullptr) {
    log_error("load_texture: failed to load '{}': {}", path,
              stbi_failure_reason());
    return TextureType{};
  }
  TextureType tex = metal_texture_detail::load_texture_from_pixels(pixels, w, h);
  stbi_image_free(pixels);
  return tex;
}

inline void unload_texture(TextureType &texture) {
  if (texture.view_id)
    sg_destroy_view({texture.view_id});
  if (texture.sampler_id)
    sg_destroy_sampler({texture.sampler_id});
  if (texture.img_id)
    sg_destroy_image({texture.img_id});
  texture = TextureType{};
}

inline void gen_texture_mipmaps(TextureType &texture) {
  // Sokol fixes an image's mip count at creation time (sg_image_desc.num_mipmaps
  // + the mip_levels[] data supplied to sg_make_image); there is no runtime
  // "generate mipmaps for an existing immutable image" call as in raylib's
  // GenTextureMipmaps. Our load_texture uploads a single mip level, so there is
  // nothing to regenerate. Documented no-op — sampling still works via
  // set_texture_filter. (To get true mipmaps here we would upload pre-built mip
  // levels at load time; out of scope for matching the current raylib API.)
  (void)texture;
}

inline void set_texture_filter(TextureType &texture, int filter) {
  if (texture.img_id == 0)
    return;
  // Recreate the sampler with the requested filter. The old sampler is
  // destroyed to avoid leaking a GPU sampler object.
  if (texture.sampler_id)
    sg_destroy_sampler({texture.sampler_id});
  sg_sampler smp = metal_texture_detail::make_sampler_for_filter(filter);
  texture.sampler_id = smp.id;
}

inline TextureType load_texture_with_color_key(const char *, const Color,
                                               const Color, int) {
  log_error("@notimplemented load_texture_with_color_key");
  return TextureType{};
}

} // namespace afterhours
