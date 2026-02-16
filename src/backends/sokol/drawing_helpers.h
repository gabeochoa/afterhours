
#pragma once

#include <cmath>
#include <bitset>

#include "../../developer.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "../../graphics.h"
#include "font_helper.h"

namespace afterhours {

namespace metal_draw_detail {
    inline void set_color(const Color c) {
        sgl_c4b(c.r, c.g, c.b, c.a);
    }
}  // namespace metal_draw_detail

inline void draw_text_ex(const afterhours::Font font, const char *content,
                         const Vector2Type position, const float font_size,
                         const float, const Color color,
                         const float = 0.0f,
                         const float = 0.0f, const float = 0.0f) {
    auto* ctx = graphics::metal_detail::g_fons_ctx;
    if (!ctx) return;
    int fid = (font.id != FONS_INVALID) ? font.id : graphics::metal_detail::g_active_font;
    if (fid == FONS_INVALID) return;
    fonsSetFont(ctx, fid);
    fonsSetSize(ctx, font_size);
    fonsSetColor(ctx, sfons_rgba(color.r, color.g, color.b, color.a));
    fonsDrawText(ctx, position.x, position.y + font_size, content, nullptr);
}

inline void draw_text(const char *content, const float x, const float y,
                      const float font_size, const Color color) {
    auto* ctx = graphics::metal_detail::g_fons_ctx;
    if (!ctx || graphics::metal_detail::g_active_font == FONS_INVALID) return;
    fonsSetFont(ctx, graphics::metal_detail::g_active_font);
    fonsSetSize(ctx, font_size);
    fonsSetColor(ctx, sfons_rgba(color.r, color.g, color.b, color.a));
    fonsDrawText(ctx, x, y + font_size, content, nullptr);
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
    draw_rectangle(RectangleType{x, y + t, t, h - 2*t}, color);   // left
    draw_rectangle(RectangleType{x + w - t, y + t, t, h - 2*t}, color); // right
}

inline void draw_rectangle_outline(const RectangleType rect, const Color color) {
    draw_rectangle_outline(rect, color, 1.0f);
}

inline void draw_rectangle_rounded(const RectangleType rect, const float, const int,
                                   const Color color, const std::bitset<4> = std::bitset<4>().reset()) {
    // TODO: actual rounded corners via triangle fans
    draw_rectangle(rect, color);
}

inline void draw_rectangle_rounded_rotated(const RectangleType rect, const float roundness, const int segments,
                                           const Color color, const std::bitset<4> corners, const float) {
    // TODO: rotation support
    draw_rectangle_rounded(rect, roundness, segments, color, corners);
}

inline void draw_rectangle_rounded_lines(const RectangleType rect, const float,
                                         const int, const Color color,
                                         const std::bitset<4> = std::bitset<4>().reset()) {
    // TODO: actual rounded corners
    draw_rectangle_outline(rect, color);
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
    // With high_dpi=false, framebuffer pixels == logical coords, no scaling needed
    sgl_scissor_rect(x, y, w, h, true);
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

inline void pop_rotation() {
    sgl_pop_matrix();
}

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
    (void)thickness;
    sgl_begin_lines();
    metal_draw_detail::set_color(color);
    sgl_v2f(start.x, start.y);
    sgl_v2f(end.x, end.y);
    sgl_end();
}

inline void draw_line_strip(Vector2Type *points, int point_count, Color color) {
    if (point_count < 2) return;
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
        float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(SEGMENTS);
        sgl_v2f(cx, cy);
        sgl_v2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
    }
    sgl_end();
}

inline void draw_circle_v(Vector2Type center, float radius, Color color) {
    draw_circle(static_cast<int>(center.x), static_cast<int>(center.y), radius, color);
}

inline void draw_circle_lines(int centerX, int centerY, float radius, Color color) {
    constexpr int SEGMENTS = 32;
    sgl_begin_line_strip();
    metal_draw_detail::set_color(color);
    float cx = static_cast<float>(centerX);
    float cy = static_cast<float>(centerY);
    for (int i = 0; i <= SEGMENTS; i++) {
        float angle = static_cast<float>(i) * 2.0f * 3.14159265f / static_cast<float>(SEGMENTS);
        sgl_v2f(cx + radius * cosf(angle), cy + radius * sinf(angle));
    }
    sgl_end();
}

inline void draw_circle_sector(Vector2Type, float, float, float, int, Color) {
    log_error("@notimplemented draw_circle_sector");
}
inline void draw_circle_sector_lines(Vector2Type, float, float, float, int, Color) {
    log_error("@notimplemented draw_circle_sector_lines");
}
inline void draw_ellipse(int, int, float, float, Color) {
    log_error("@notimplemented draw_ellipse");
}
inline void draw_ellipse_lines(int, int, float, float, Color) {
    log_error("@notimplemented draw_ellipse_lines");
}

inline void draw_triangle(Vector2Type v1, Vector2Type v2, Vector2Type v3, Color color) {
    sgl_begin_triangles();
    metal_draw_detail::set_color(color);
    sgl_v2f(v1.x, v1.y);
    sgl_v2f(v2.x, v2.y);
    sgl_v2f(v3.x, v3.y);
    sgl_end();
}

inline void draw_triangle_lines(Vector2Type v1, Vector2Type v2, Vector2Type v3, Color color) {
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

}  // namespace afterhours
