// Graphics Backend Abstraction for Afterhours
// Provides a backend-agnostic graphics interface.
// Gap 07: Renderer Abstraction
//
// Currently implements raylib backend only.

#pragma once

#include <cstdint>

// Include raylib
#ifdef AFTER_HOURS_USE_RAYLIB
#include "developer.h"  // For raylib:: namespace
#endif

namespace afterhours {
namespace graphics {

//=============================================================================
// TYPE ALIASES (use afterhours::Color from color.h instead)
//=============================================================================

#ifdef AFTER_HOURS_USE_RAYLIB
using Color = raylib::Color;
using Vec2 = raylib::Vector2;
using Rect = raylib::Rectangle;
#else
struct Color { unsigned char r, g, b, a; };
struct Vec2 { float x, y; };
struct Rect { float x, y, width, height; };
#endif

//=============================================================================
// SCISSOR/CLIPPING
//=============================================================================

inline void begin_scissor_mode(int x, int y, int width, int height) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::BeginScissorMode(x, y, width, height);
#endif
}

inline void end_scissor_mode() {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::EndScissorMode();
#endif
}

//=============================================================================
// MATRIX OPERATIONS
//=============================================================================

inline void push_matrix() {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::rlPushMatrix();
#endif
}

inline void pop_matrix() {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::rlPopMatrix();
#endif
}

inline void translate(float x, float y) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::rlTranslatef(x, y, 0.0f);
#endif
}

//=============================================================================
// DRAWING PRIMITIVES
//=============================================================================

inline void draw_rectangle(float x, float y, float width, float height, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawRectangle(static_cast<int>(x), static_cast<int>(y),
                        static_cast<int>(width), static_cast<int>(height), color);
#else
  (void)x; (void)y; (void)width; (void)height; (void)color;
#endif
}

inline void draw_rectangle_rec(Rect rect, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawRectangleRec(rect, color);
#else
  (void)rect; (void)color;
#endif
}

inline void draw_rectangle_lines(float x, float y, float width, float height, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawRectangleLines(static_cast<int>(x), static_cast<int>(y),
                             static_cast<int>(width), static_cast<int>(height), color);
#else
  (void)x; (void)y; (void)width; (void)height; (void)color;
#endif
}

inline void draw_line(float x1, float y1, float x2, float y2, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawLine(static_cast<int>(x1), static_cast<int>(y1),
                   static_cast<int>(x2), static_cast<int>(y2), color);
#else
  (void)x1; (void)y1; (void)x2; (void)y2; (void)color;
#endif
}

inline void draw_line_ex(Vec2 start, Vec2 end, float thick, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawLineEx(start, end, thick, color);
#else
  (void)start; (void)end; (void)thick; (void)color;
#endif
}

inline void draw_circle(float cx, float cy, float radius, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawCircle(static_cast<int>(cx), static_cast<int>(cy), radius, color);
#else
  (void)cx; (void)cy; (void)radius; (void)color;
#endif
}

inline void draw_ellipse(float cx, float cy, float rx, float ry, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawEllipse(static_cast<int>(cx), static_cast<int>(cy), rx, ry, color);
#else
  (void)cx; (void)cy; (void)rx; (void)ry; (void)color;
#endif
}

inline void draw_triangle(Vec2 v1, Vec2 v2, Vec2 v3, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawTriangle(v1, v2, v3, color);
#else
  (void)v1; (void)v2; (void)v3; (void)color;
#endif
}

inline void draw_triangle_lines(Vec2 v1, Vec2 v2, Vec2 v3, Color color) {
#ifdef AFTER_HOURS_USE_RAYLIB
  raylib::DrawTriangleLines(v1, v2, v3, color);
#else
  (void)v1; (void)v2; (void)v3; (void)color;
#endif
}

} // namespace graphics
} // namespace afterhours
