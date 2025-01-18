
#pragma once

#include "developer.h"
#include "font_helper.h"
#include "plugins/color.h"

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB

inline void draw_text_ex(raylib::Font font, const char *content,
                         Vector2Type position, float font_size, float spacing,
                         Color color) {
  raylib::DrawTextEx(font, content, position, font_size, spacing, color);
}
inline void draw_text(const char *content, float x, float y, float font_size,
                      Color color) {
  raylib::DrawText(content, x, y, font_size, color);
}

inline void draw_rectangle_outline(RectangleType rect, Color color) {
  raylib::DrawRectangleLinesEx(rect, 3.f, color);
}

inline void draw_rectangle(RectangleType rect, Color color) {
  raylib::DrawRectangleRec(rect, color);
}

inline void
draw_rectangle_rounded(RectangleType rect, float roundness, int segments,
                       Color color,
                       std::bitset<4> corners = std::bitset<4>().reset()) {
  if (corners.all()) {
    raylib::DrawRectangleRounded(rect, roundness, segments, color);
    return;
  }

  if (corners.none()) {
    draw_rectangle(rect, color);
    return;
  }

  // TODO
  draw_rectangle(rect, color);
}

inline raylib::Font get_default_font() { return raylib::GetFontDefault(); }
inline raylib::Font get_unset_font() { return raylib::GetFontDefault(); }

#else
inline void draw_text_ex(afterhours::Font, const char *, Vector2Type, float,
                         float, Color) {}
inline void draw_text(const char *, float, float, float, Color) {}
inline void draw_rectangle(RectangleType, Color) {}
inline void draw_rectangle_outline(RectangleType, Color) {}
inline void draw_rectangle_rounded(RectangleType, float, int, Color,
                                   std::bitset<4>) {}
inline afterhours::Font get_default_font() { return afterhours::Font(); }
inline afterhours::Font get_unset_font() { return afterhours::Font(); }
#endif

} // namespace afterhours
