
#pragma once

#include <algorithm>
#include <bitset>

#include "config.h"

// Rect geometry lives beside the draw calls that consume it.
#include "rect.h"

#ifdef AFTER_HOURS_USE_RAYLIB
#include "backends/raylib/drawing_helpers.h"
#elif defined(AFTER_HOURS_SOKOL)
#include "backends/sokol/drawing_helpers.h"
#elif defined(AFTER_HOURS_USE_PSX)
#define AFTER_HOURS_BACKEND_PSX
#include "backends/psx/drawing_helpers.h"
#else
// The `none` backend records draw calls rather than issuing them (see its
// drawing_helpers.h). Callers that want to assert on what was drawn -- tests,
// mostly -- can key off this instead of re-deriving the backend condition.
#define AFTER_HOURS_BACKEND_NONE
#include "backends/none/drawing_helpers.h"
#endif

namespace afterhours {

// Text layout for raw (non-UI) drawing. Anchors: centered takes the text
// centre, right-aligned takes the text's right edge, with_shadow takes the
// text's top-left and draws the shadow first. For centred + shadow,
// measure with measure_text and offset the with_shadow call by half the
// width. Spacing matches draw_text_ex.
inline void draw_text_centered(const Font font, const char *text,
                               const float center_x, const float y,
                               const float size, const Color color,
                               const float spacing = 1.f) {
  if (!text)
    return;
  const float w = measure_text(font, text, size, spacing).x;
  draw_text_ex(font, text, Vector2Type{center_x - w * 0.5f, y}, size, spacing,
               color);
}

inline void draw_text_right_aligned(const Font font, const char *text,
                                    const float right_x, const float y,
                                    const float size, const Color color,
                                    const float spacing = 1.f) {
  if (!text)
    return;
  const float w = measure_text(font, text, size, spacing).x;
  draw_text_ex(font, text, Vector2Type{right_x - w, y}, size, spacing, color);
}

inline void draw_text_with_shadow(const Font font, const char *text,
                                  const float x, const float y,
                                  const float size, const Color color,
                                  const Color shadow_color,
                                  const float shadow_offset_x = 2.f,
                                  const float shadow_offset_y = 2.f,
                                  const float spacing = 1.f) {
  if (!text)
    return;
  draw_text_ex(font, text,
               Vector2Type{x + shadow_offset_x, y + shadow_offset_y}, size,
               spacing, shadow_color);
  draw_text_ex(font, text, Vector2Type{x, y}, size, spacing, color);
}

// Rounded rects sized in pixels, not in `with_roundness` fractions of the
// short side: the same `px` radius on a row and on a panel. `roundness` in
// the backend calls is a fraction of half the short side, and the default
// corner mask is empty (a square fill that silently discards the radius),
// so these convert and always pass the all-corners mask.
inline float roundness_for_px(const RectangleType rect, const float px) {
  const float s = std::min(rect.width, rect.height);
  return s <= 0.f ? 0.f : std::min(0.5f, px * 2.f / s);
}

inline void draw_rectangle_rounded_px(const RectangleType rect,
                                        const float px, const Color color,
                                        const int segments = 8) {
  draw_rectangle_rounded(rect, roundness_for_px(rect, px), segments, color,
                         std::bitset<4>().set());
}

inline void draw_rectangle_rounded_lines_px(const RectangleType rect,
                                              const float px,
                                              const float thickness,
                                              const Color color,
                                              const int segments = 8) {
  draw_rectangle_rounded_lines_ex(rect, roundness_for_px(rect, px), segments,
                                  thickness, color);
}

} // namespace afterhours
