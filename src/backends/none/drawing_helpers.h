
#pragma once

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../../developer.h"
#include "../../graphics_common.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "font_helper.h"

namespace afterhours {

// ---------------------------------------------------------------------------
// Draw recording
//
// This backend exists so the library builds and runs with no GPU. Logging an
// error per draw call made it useless for testing the render path, which is a
// large part of why rendering.h had no coverage at all -- the imm test harness
// only ever ran autolayout.
//
// Recording instead costs nothing and lets a test assert on what was drawn
// (which text, which colour, which rect) with no window, no font, no GPU. Only
// the ops the UI render path actually emits are recorded; the rest still log,
// because an unimplemented call reached by accident should still be loud.
//
// Tests are responsible for clearing between frames -- see clear_draw_calls().
// ---------------------------------------------------------------------------
struct DrawCall {
  std::string op;
  RectangleType rect{};
  Color color{};
  std::string text;
};

inline std::vector<DrawCall> &draw_calls() {
  static std::vector<DrawCall> calls;
  return calls;
}

inline void clear_draw_calls() { draw_calls().clear(); }

inline void draw_text_ex(const afterhours::Font, const char *text,
                         const Vector2Type position, const float,
                         const float, const Color color, const float = 0.0f,
                         const float = 0.0f, const float = 0.0f) {
  draw_calls().push_back({"text",
                          RectangleType{position.x, position.y, 0.f, 0.f},
                          color,
                          text ? text : ""});
}
inline void draw_text(const char *text, const float x, const float y,
                      const float, const Color color) {
  draw_calls().push_back(
      {"text", RectangleType{x, y, 0.f, 0.f}, color, text ? text : ""});
}
inline void draw_rectangle(const RectangleType rect, const Color color) {
  draw_calls().push_back({"rectangle", rect, color, ""});
}
inline void draw_rectangle_outline(const RectangleType rect,
                                   const Color color) {
  draw_calls().push_back({"rectangle_outline", rect, color, ""});
}
inline void draw_rectangle_outline(const RectangleType rect, const Color color,
                                   const float) {
  draw_calls().push_back({"rectangle_outline", rect, color, ""});
}
inline void draw_rectangle_rounded(const RectangleType rect, const float,
                                   const int, const Color color,
                                   const std::bitset<4>) {
  draw_calls().push_back({"rectangle_rounded", rect, color, ""});
}
inline void draw_rectangle_rounded_rotated(const RectangleType, const float,
                                           const int, const Color,
                                           const std::bitset<4>, const float) {
  log_error("@notimplemented draw_rectangle_rounded_rotated");
}
// Recorded, not stubbed: this is the call every outline goes through --
// borders, and the focus ring. While it logged and dropped, no test could
// observe either, which is why the ring had no coverage at all.
inline void draw_rectangle_rounded_lines(const RectangleType rect, const float,
                                         const int, const Color color,
                                         const std::bitset<4>) {
  draw_calls().push_back({"rectangle_rounded_lines", rect, color, ""});
}
inline void draw_rectangle_rounded_lines_ex(const RectangleType rect,
                                            const float, const int,
                                            const float, const Color color) {
  draw_calls().push_back({"rectangle_rounded_lines", rect, color, ""});
}
inline void draw_rectangle_gradient_v(const RectangleType, const Color,
                                      const Color) {
  log_error("@notimplemented draw_rectangle_gradient_v");
}
inline void draw_rectangle_gradient_h(const RectangleType, const Color,
                                      const Color) {
  log_error("@notimplemented draw_rectangle_gradient_h");
}
inline void draw_rectangle_gradient_ex(const RectangleType, const Color,
                                       const Color, const Color, const Color) {
  log_error("@notimplemented draw_rectangle_gradient_ex");
}
inline void draw_circle_gradient(int, int, float, Color, Color) {
  log_error("@notimplemented draw_circle_gradient");
}
inline void draw_texture_npatch(const texture_manager::Texture,
                                const RectangleType, int, int, int, int,
                                const Color) {
  log_error("@notimplemented draw_texture_npatch");
}
inline void draw_ring_segment(float, float, float, float, float, float, int,
                              Color) {
  log_error("@notimplemented draw_ring_segment");
}
inline void draw_ring(float, float, float, float, int, Color) {
  log_error("@notimplemented draw_ring");
}
inline void begin_scissor_mode(int, int, int, int) {
  log_error("@notimplemented begin_scissor_mode");
}
inline void end_scissor_mode() {
  log_error("@notimplemented end_scissor_mode");
}
inline void push_rotation(float, float, float) {
  log_error("@notimplemented push_rotation");
}
inline void pop_rotation() { log_error("@notimplemented pop_rotation"); }
inline void draw_line(int, int, int, int, Color) {
  log_error("@notimplemented draw_line");
}
inline void draw_line_ex(Vector2Type, Vector2Type, float, Color) {
  log_error("@notimplemented draw_line_ex");
}
inline void draw_line_strip(Vector2Type *, int, Color) {
  log_error("@notimplemented draw_line_strip");
}
inline bool check_collision_point_line(Vector2Type, Vector2Type, Vector2Type,
                                       float) {
  log_error("@notimplemented check_collision_point_line");
  return false;
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
inline void draw_circle(int, int, float, Color) {
  log_error("@notimplemented draw_circle");
}
inline void draw_circle_v(Vector2Type, float, Color) {
  log_error("@notimplemented draw_circle_v");
}
inline void draw_circle_lines(int, int, float, Color) {
  log_error("@notimplemented draw_circle_lines");
}
inline void draw_circle_sector(Vector2Type, float, float, float, int, Color) {
  log_error("@notimplemented draw_circle_sector");
}
inline void draw_circle_sector_lines(Vector2Type, float, float, float, int,
                                     Color) {
  log_error("@notimplemented draw_circle_sector_lines");
}
inline void draw_ellipse(int, int, float, float, Color) {
  log_error("@notimplemented draw_ellipse");
}
inline void draw_ellipse_lines(int, int, float, float, Color) {
  log_error("@notimplemented draw_ellipse_lines");
}
inline void draw_triangle(Vector2Type, Vector2Type, Vector2Type, Color) {
  log_error("@notimplemented draw_triangle");
}
inline void draw_triangle_lines(Vector2Type, Vector2Type, Vector2Type, Color) {
  log_error("@notimplemented draw_triangle_lines");
}
inline void draw_poly(Vector2Type, int, float, float, Color) {
  log_error("@notimplemented draw_poly");
}
inline void draw_poly_lines(Vector2Type, int, float, float, Color) {
  log_error("@notimplemented draw_poly_lines");
}
inline void draw_poly_lines_ex(Vector2Type, int, float, float, float, Color) {
  log_error("@notimplemented draw_poly_lines_ex");
}
inline void set_mouse_cursor(int) {}

inline afterhours::Font get_default_font() { return afterhours::Font(); }
inline afterhours::Font get_unset_font() { return afterhours::Font(); }

inline graphics::RenderTextureType load_render_texture(int, int) {
  log_error("@notimplemented load_render_texture");
  return {};
}

inline void unload_render_texture(graphics::RenderTextureType &) {
  log_error("@notimplemented unload_render_texture");
}

inline void begin_texture_mode(graphics::RenderTextureType &) {
  log_error("@notimplemented begin_texture_mode");
}

inline void end_texture_mode() {
  log_error("@notimplemented end_texture_mode");
}

inline void draw_render_texture(const graphics::RenderTextureType &, float,
                                float, Color) {
  log_error("@notimplemented draw_render_texture");
}

inline void draw_texture_rec(TextureType, RectangleType, Vector2Type, Color) {
  log_error("@notimplemented draw_texture_rec");
}
inline void draw_texture_pro(TextureType, RectangleType, RectangleType,
                             Vector2Type, float, Color) {
  log_error("@notimplemented draw_texture_pro");
}

inline bool capture_render_texture(const graphics::RenderTextureType &,
                                   const std::filesystem::path &) {
  log_error("@notimplemented capture_render_texture");
  return false;
}

inline std::vector<uint8_t>
capture_render_texture_to_memory(const graphics::RenderTextureType &) {
  log_error("@notimplemented capture_render_texture_to_memory");
  return {};
}

inline std::vector<uint8_t> capture_screen_to_memory() {
  log_error("@notimplemented capture_screen_to_memory");
  return {};
}

inline constexpr int TEXTURE_FILTER_BILINEAR = 1;
inline constexpr int TEXTURE_FILTER_TRILINEAR = 2;

inline TextureType load_texture(const char *) {
  log_error("@notimplemented load_texture");
  return TextureType{};
}

inline void unload_texture(TextureType &) {
  log_error("@notimplemented unload_texture");
}

inline void gen_texture_mipmaps(TextureType &) {
  log_error("@notimplemented gen_texture_mipmaps");
}

inline void set_texture_filter(TextureType &, int) {
  log_error("@notimplemented set_texture_filter");
}

inline TextureType load_texture_with_color_key(const char *, const Color,
                                               const Color, int) {
  log_error("@notimplemented load_texture_with_color_key");
  return TextureType{};
}

} // namespace afterhours
