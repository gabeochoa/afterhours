
#pragma once
#include "../../render_capture.h"

#include "../../blend.h"

#include <algorithm>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "../../capture.h"
#include "../../developer.h"
#include "../../graphics_common.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "budget.h"
#include "font_helper.h"
#include "image_decode.h"

namespace afterhours {

using DrawCall = capture::DrawnCall;

inline std::vector<DrawCall> &draw_calls() { return capture::calls(); }

inline void clear_draw_calls() { capture::clear(); }

inline const bool capture_always_on = (capture::enable(), true);

inline void draw_text_ex(const afterhours::Font, const char *text,
                         const Vector2Type position, const float font_size,
                         const float, const Color color, const float = 0.0f,
                         const float = 0.0f, const float = 0.0f) {
  capture::record("text", RectangleType{position.x, position.y, 0.f, font_size},
                  color, text ? text : "");
  psx::budget::add_primitives(text ? std::strlen(text) : 0);
}
inline void draw_text(const char *text, const float x, const float y,
                      const float font_size, const Color color) {
  capture::record("text", RectangleType{x, y, 0.f, font_size}, color,
                  text ? text : "");
  psx::budget::add_primitives(text ? std::strlen(text) : 0);
}
inline void draw_rectangle(const RectangleType rect, const Color color) {
  capture::record("rectangle", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_outline(const RectangleType rect,
                                   const Color color) {
  capture::record("rectangle_outline", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_outline(const RectangleType rect, const Color color,
                                   const float) {
  capture::record("rectangle_outline", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_rounded(const RectangleType rect, const float,
                                   const int, const Color color,
                                   const std::bitset<4>) {
  capture::record("rectangle_rounded", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_rounded_rotated(const RectangleType rect, const float,
                                           const int, const Color color,
                                           const std::bitset<4>, const float) {
  capture::record("rectangle_rounded", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_rounded_lines(const RectangleType rect, const float,
                                         const int, const Color color,
                                         const std::bitset<4>) {
  capture::record("rectangle_rounded_lines", rect, color);
  psx::budget::add_primitives(1);
}
inline void draw_rectangle_rounded_lines_ex(const RectangleType rect,
                                            const float, const int,
                                            const float, const Color color) {
  capture::record("rectangle_rounded_lines", rect, color);
  psx::budget::add_primitives(1);
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
inline void set_blend_mode(blend::Mode mode) {
  if (mode == blend::detail::current_mode())
    return;
  blend::detail::current_mode() = mode;
  blend::detail::transition_count()++;
  capture::record("blend", RectangleType{}, ColorType{});
}

struct blend_scope {
  blend::Mode previous;
  explicit blend_scope(blend::Mode mode) : previous(blend::current()) {
    set_blend_mode(mode);
  }
  ~blend_scope() { set_blend_mode(previous); }
  blend_scope(const blend_scope &) = delete;
  blend_scope &operator=(const blend_scope &) = delete;
};

inline void begin_shader_mode(graphics::ShaderType &) {}
inline void end_shader_mode() {}

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
inline void draw_line_ex(Vector2Type start, Vector2Type end, float thickness,
                         Color color) {
  capture::record("line", RectangleType{start.x, start.y, end.x - start.x,
                                        end.y - start.y},
                  color, "");
  psx::budget::add_primitives(1);
  (void)thickness;
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
inline void draw_quad(const Vector2Type (&)[4], Color) {
  psx::budget::add_primitives(1);
}
inline void draw_texture_quad(TextureType, const Vector2Type (&)[4], Color) {
  psx::budget::add_primitives(1);
}

inline void draw_triangle(Vector2Type a, Vector2Type b, Vector2Type c,
                          Color color) {
  const float x0 = std::min({a.x, b.x, c.x}), x1 = std::max({a.x, b.x, c.x});
  const float y0 = std::min({a.y, b.y, c.y}), y1 = std::max({a.y, b.y, c.y});
  capture::record("triangle", RectangleType{x0, y0, x1 - x0, y1 - y0}, color);
  psx::budget::add_primitives(1);
}
inline void draw_triangle_lines(Vector2Type a, Vector2Type b, Vector2Type c,
                                Color color) {
  draw_triangle(a, b, c, color);
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

namespace detail {
inline afterhours::Font &default_font_override() {
  static afterhours::Font font{};
  return font;
}
}

inline void set_default_font(afterhours::Font font) {
  detail::default_font_override() = font;
}
inline void clear_default_font() { detail::default_font_override() = {}; }
inline afterhours::Font get_default_font() {
  return detail::default_font_override();
}
inline afterhours::Font get_unset_font() { return get_default_font(); }

inline graphics::RenderTextureType load_render_texture(int w, int h) {
  graphics::RenderTextureType rt;
  rt.width = w;
  rt.height = h;
  const psx::budget::TextureAlloc alloc =
      psx::budget::alloc_render_target(w, h, "render texture");
  rt.budget_id = alloc.id;
  return rt;
}

inline void unload_render_texture(graphics::RenderTextureType &rt) {
  psx::budget::free_texture(rt.budget_id);
  rt = {};
}

inline void begin_texture_mode(graphics::RenderTextureType &) {
  log_error("@notimplemented begin_texture_mode");
}

inline void end_texture_mode() {
  log_error("@notimplemented end_texture_mode");
}

inline void copy_screen_to_render_texture(graphics::RenderTextureType &, RectangleType) {
  log_error("@notimplemented copy_screen_to_render_texture");
}

inline void draw_render_texture(const graphics::RenderTextureType &, float,
                                float, Color) {
  log_error("@notimplemented draw_render_texture");
}

inline void draw_texture_rec(TextureType, RectangleType source,
                             Vector2Type position, Color color) {
  capture::record("texture",
                  RectangleType{position.x, position.y, source.width,
                                source.height},
                  color);
  psx::budget::add_primitives(1);
}
inline void draw_texture_pro(TextureType, RectangleType, RectangleType dest,
                             Vector2Type, float, Color color) {
  capture::record("texture", dest, color);
  psx::budget::add_primitives(1);
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

inline std::optional<RGBACapture>
capture_render_texture_rgba(const graphics::RenderTextureType &) { return std::nullopt; }

inline std::optional<PNGCapture>
capture_render_texture_png(const graphics::RenderTextureType &) { return std::nullopt; }

inline std::vector<uint8_t> capture_screen_to_memory() {
  log_error("@notimplemented capture_screen_to_memory");
  return {};
}

inline constexpr int TEXTURE_FILTER_BILINEAR = 1;
inline constexpr int TEXTURE_FILTER_TRILINEAR = 2;
inline constexpr int TEXTURE_WRAP_REPEAT = 0;
inline constexpr int TEXTURE_WRAP_CLAMP = 1;
inline constexpr int TEXTURE_WRAP_MIRROR_REPEAT = 2;

inline TextureType make_budgeted_texture(int w, int h, const char *name) {
  const psx::budget::TextureAlloc alloc =
      psx::budget::alloc_texture(w, h, name);
  if (psx::budget::config().strict && !alloc.ok)
    return TextureType{};
  TextureType tex;
  tex.width = static_cast<float>(w);
  tex.height = static_cast<float>(h);
  tex.budget_id = alloc.id;
  return tex;
}

inline TextureType load_texture(const char *path) {
  int w = 0, h = 0, comp = 0;
  if (path == nullptr || !stbi_info(path, &w, &h, &comp)) {
    log_warn("psx load_texture: cannot read image header for '{}'",
             path ? path : "<null>");
    return TextureType{};
  }
  return make_budgeted_texture(w, h, path);
}

inline TextureType load_texture_from_rgba(const std::uint8_t *, int w, int h,
                                          int = TEXTURE_FILTER_BILINEAR) {
  const std::string label =
      "<rgba " + std::to_string(w) + "x" + std::to_string(h) + ">";
  return make_budgeted_texture(w, h, label.c_str());
}

inline void unload_texture(TextureType &tex) {
  psx::budget::free_texture(tex.budget_id);
  tex = TextureType{};
}

inline void gen_texture_mipmaps(TextureType &) {
  log_error("@notimplemented gen_texture_mipmaps (no console equivalent)");
}

inline void set_texture_filter(TextureType &, int) {
  log_error("@notimplemented set_texture_filter (no console equivalent)");
}

inline void set_texture_wrap(TextureType &, int) {}

inline void draw_fps(int, int) {}

inline TextureType load_texture_with_color_key(const char *path, const Color,
                                               const Color, int) {
  return load_texture(path);
}

}
