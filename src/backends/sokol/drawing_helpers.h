
#pragma once

#include <bitset>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

#include "../../developer.h"
#include "../../graphics.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "font_helper.h"

namespace afterhours {

namespace metal_draw_detail {
inline void set_color(const Color c) { sgl_c4b(c.r, c.g, c.b, c.a); }
} // namespace metal_draw_detail

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
                               const std::bitset<4> corners, const float) {
  // TODO: rotation support
  log_warn("draw_rectangle_rounded_rotated: rotation not implemented in sokol "
           "backend");
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
  // TODO: thick lines via quads
  log_warn("draw_line_ex: thick lines not implemented in sokol backend, "
           "ignoring thickness");
  (void)thickness;
  sgl_begin_lines();
  metal_draw_detail::set_color(color);
  sgl_v2f(start.x, start.y);
  sgl_v2f(end.x, end.y);
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

inline void draw_poly(Vector2Type, int, float, float, Color) {
  log_error("@notimplemented draw_poly");
}
inline void draw_poly_lines(Vector2Type, int, float, float, Color) {
  log_error("@notimplemented draw_poly_lines");
}
inline void draw_poly_lines_ex(Vector2Type, int, float, float, float, Color) {
  log_error("@notimplemented draw_poly_lines_ex");
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
  // If this render texture's pass is already active, continue drawing
  // into the existing pass (matches Raylib's begin_texture_mode behavior).
  if (graphics::metal_detail::g_pass_active &&
      graphics::metal_detail::g_active_rt_color_view_id == rt.color_view_id) {
    return;
  }

  // Flush and end the current pass (swapchain or another offscreen)
  if (graphics::metal_detail::g_pass_active) {
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
  if (graphics::metal_detail::g_pass_active) {
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
}

inline void draw_texture_rec(TextureType, RectangleType, Vector2Type, Color) {
  log_warn("draw_texture_rec: not yet implemented in sokol backend");
}

inline bool capture_render_texture(const graphics::RenderTextureType &,
                                   const std::filesystem::path &) {
  log_warn("capture_render_texture: not yet implemented in sokol backend");
  return false;
}

inline std::vector<uint8_t>
capture_render_texture_to_memory(const graphics::RenderTextureType &) {
  log_warn("capture_render_texture_to_memory: not yet implemented in sokol "
           "backend");
  return {};
}

inline std::vector<uint8_t> capture_screen_to_memory() {
  log_warn("capture_screen_to_memory: not yet implemented in sokol backend");
  return {};
}

} // namespace afterhours
