
#pragma once

#include <bitset>

#include "../../developer.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "font_helper.h"

namespace afterhours {

inline void draw_text_ex(const afterhours::Font, const char *,
                         const Vector2Type, const float, const float,
                         const Color, const float = 0.0f,
                         const float = 0.0f, const float = 0.0f) { log_error("@notimplemented draw_text_ex"); }
inline void draw_text(const char *, const float, const float, const float,
                      const Color) { log_error("@notimplemented draw_text"); }
inline void draw_rectangle(const RectangleType, const Color) { log_error("@notimplemented draw_rectangle"); }
inline void draw_rectangle_outline(const RectangleType, const Color) { log_error("@notimplemented draw_rectangle_outline"); }
inline void draw_rectangle_outline(const RectangleType, const Color,
                                   const float) { log_error("@notimplemented draw_rectangle_outline"); }
inline void draw_rectangle_rounded(const RectangleType, const float, const int,
                                   const Color, const std::bitset<4>) { log_error("@notimplemented draw_rectangle_rounded"); }
inline void draw_rectangle_rounded_rotated(const RectangleType, const float, const int,
                                           const Color, const std::bitset<4>, const float) { log_error("@notimplemented draw_rectangle_rounded_rotated"); }
inline void draw_rectangle_rounded_lines(const RectangleType, const float,
                                         const int, const Color,
                                         const std::bitset<4>) { log_error("@notimplemented draw_rectangle_rounded_lines"); }
inline void draw_texture_npatch(const texture_manager::Texture,
                                const RectangleType, int, int, int, int,
                                const Color) { log_error("@notimplemented draw_texture_npatch"); }
inline void draw_ring_segment(float, float, float, float, float, float, int,
                              Color) { log_error("@notimplemented draw_ring_segment"); }
inline void draw_ring(float, float, float, float, int, Color) { log_error("@notimplemented draw_ring"); }
inline void begin_scissor_mode(int, int, int, int) { log_error("@notimplemented begin_scissor_mode"); }
inline void end_scissor_mode() { log_error("@notimplemented end_scissor_mode"); }
inline void push_rotation(float, float, float) { log_error("@notimplemented push_rotation"); }
inline void pop_rotation() { log_error("@notimplemented pop_rotation"); }
inline void draw_line(int, int, int, int, Color) { log_error("@notimplemented draw_line"); }
inline void draw_line_ex(Vector2Type, Vector2Type, float, Color) { log_error("@notimplemented draw_line_ex"); }
inline void draw_line_strip(Vector2Type *, int, Color) { log_error("@notimplemented draw_line_strip"); }
inline void draw_circle(int, int, float, Color) { log_error("@notimplemented draw_circle"); }
inline void draw_circle_v(Vector2Type, float, Color) { log_error("@notimplemented draw_circle_v"); }
inline void draw_circle_lines(int, int, float, Color) { log_error("@notimplemented draw_circle_lines"); }
inline void draw_circle_sector(Vector2Type, float, float, float, int, Color) { log_error("@notimplemented draw_circle_sector"); }
inline void draw_circle_sector_lines(Vector2Type, float, float, float, int,
                                     Color) { log_error("@notimplemented draw_circle_sector_lines"); }
inline void draw_ellipse(int, int, float, float, Color) { log_error("@notimplemented draw_ellipse"); }
inline void draw_ellipse_lines(int, int, float, float, Color) { log_error("@notimplemented draw_ellipse_lines"); }
inline void draw_triangle(Vector2Type, Vector2Type, Vector2Type, Color) { log_error("@notimplemented draw_triangle"); }
inline void draw_triangle_lines(Vector2Type, Vector2Type, Vector2Type, Color) { log_error("@notimplemented draw_triangle_lines"); }
inline void draw_poly(Vector2Type, int, float, float, Color) { log_error("@notimplemented draw_poly"); }
inline void draw_poly_lines(Vector2Type, int, float, float, Color) { log_error("@notimplemented draw_poly_lines"); }
inline void draw_poly_lines_ex(Vector2Type, int, float, float, float, Color) { log_error("@notimplemented draw_poly_lines_ex"); }
inline void set_mouse_cursor(int) {}

inline afterhours::Font get_default_font() { return afterhours::Font(); }
inline afterhours::Font get_unset_font() { return afterhours::Font(); }

}  // namespace afterhours
