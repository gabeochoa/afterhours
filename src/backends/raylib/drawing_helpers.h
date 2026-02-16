
#pragma once

#include <cmath>
#include <bitset>

#include "../../developer.h"
#include "../../plugins/color.h"
#include "../../plugins/texture_manager.h"
#include "../../graphics.h"
#include "font_helper.h"

namespace raylib {
#define SMOOTH_CIRCLE_ERROR_RATE 0.5f
static Texture2D texShapes = {1, 1, 1, 1, 7};

// AI
inline int CalculateSegments(const float radius) {
    const float th =
        acosf(2 * powf(1 - SMOOTH_CIRCLE_ERROR_RATE / radius, 2) - 1);
    return (int) (ceilf(2 * PI / th) / 4.0f);
}

inline void DrawCorner(const float x, const float y, const float radius,
                       const int segments, const Color color, float angle) {
    const float stepLength = 90.0f / (float) segments;

    rlBegin(RL_TRIANGLES);
    rlColor4ub(color.r, color.g, color.b, color.a);

    for (int i = 0; i < segments; i++) {
        rlVertex2f(x, y);
        rlVertex2f(x + cosf(DEG2RAD * (angle + stepLength)) * radius,
                   y + sinf(DEG2RAD * (angle + stepLength)) * radius);
        rlVertex2f(x + cosf(DEG2RAD * angle) * radius,
                   y + sinf(DEG2RAD * angle) * radius);
        angle += stepLength;
    }
    rlEnd();
}

inline void DrawRectangleCustom(const Rectangle rec,
                                const float roundnessBottomRight,
                                const float roundnessBottomLeft,
                                const float roundnessTopRight,
                                const float roundnessTopLeft,
                                const int segments, const Color color) {
    // Calculate corner radius for each corner
    const float radiusBottomRight =
        (rec.width > rec.height) ? (rec.height * roundnessBottomRight) / 2
                                 : (rec.width * roundnessBottomRight) / 2;
    const float radiusBottomLeft = (rec.width > rec.height)
                                       ? (rec.height * roundnessBottomLeft) / 2
                                       : (rec.width * roundnessBottomLeft) / 2;
    const float radiusTopRight = (rec.width > rec.height)
                                     ? (rec.height * roundnessTopRight) / 2
                                     : (rec.width * roundnessTopRight) / 2;
    const float radiusTopLeft = (rec.width > rec.height)
                                    ? (rec.height * roundnessTopLeft) / 2
                                    : (rec.width * roundnessTopLeft) / 2;

    // Calculate number of segments to use for each corner
    const int segmentsBottomRight =
        (segments < 4) ? CalculateSegments(radiusBottomRight) : segments;
    const int segmentsBottomLeft =
        (segments < 4) ? CalculateSegments(radiusBottomLeft) : segments;
    const int segmentsTopRight =
        (segments < 4) ? CalculateSegments(radiusTopRight) : segments;
    const int segmentsTopLeft =
        (segments < 4) ? CalculateSegments(radiusTopLeft) : segments;

    // Draw the main rectangle (excluding corner areas)
    const float leftX = rec.x + radiusTopLeft;
    const float rightX =
        rec.x + rec.width -
        (radiusTopRight > 0 ? radiusTopRight : radiusBottomRight);
    const float topY = rec.y + radiusTopLeft;
    const float bottomY = rec.y + rec.height - radiusBottomLeft;

    // Center rectangle
    if (leftX < rightX && topY < bottomY) {
        DrawRectangleRec(Rectangle{leftX, topY, rightX - leftX, bottomY - topY},
                         color);
    }

    // Top rectangle (between top corners)
    if (radiusTopLeft > 0 || radiusTopRight > 0) {
        const float topRectLeft = rec.x + radiusTopLeft;
        const float topRectRight = rec.x + rec.width - radiusTopRight;
        if (topRectLeft < topRectRight) {
            DrawRectangleRec(
                Rectangle{topRectLeft, rec.y, topRectRight - topRectLeft,
                          radiusTopLeft},
                color);
        }
    }

    // Bottom rectangle (between bottom corners)
    if (radiusBottomLeft > 0 || radiusBottomRight > 0) {
        const float bottomRectLeft = rec.x + radiusBottomLeft;
        const float bottomRectRight = rec.x + rec.width - radiusBottomRight;
        if (bottomRectLeft < bottomRectRight) {
            DrawRectangleRec(
                Rectangle{bottomRectLeft, rec.y + rec.height - radiusBottomLeft,
                          bottomRectRight - bottomRectLeft, radiusBottomLeft},
                color);
        }
    }

    // Left rectangle (between left corners)
    if (radiusTopLeft > 0 || radiusBottomLeft > 0) {
        const float leftRectTop = rec.y + radiusTopLeft;
        const float leftRectBottom = rec.y + rec.height - radiusBottomLeft;
        if (leftRectTop < leftRectBottom) {
            DrawRectangleRec(Rectangle{rec.x, leftRectTop, radiusTopLeft,
                                       leftRectBottom - leftRectTop},
                             color);
        }
    }

    // Right rectangle (between right corners)
    if (radiusBottomRight > 0) {
        const float rightRectTop = rec.y + radiusTopRight;
        const float rightRectBottom = rec.y + rec.height - radiusBottomRight;
        DrawRectangleRec(
            Rectangle{rec.x + rec.width - radiusBottomRight, rightRectTop,
                      radiusBottomRight, rightRectBottom - rightRectTop},
            color);
    }

    if (radiusTopRight > 0) {
        const float rightRectTop = rec.y + radiusTopRight;
        const float rightRectBottom = rec.y + rec.height - radiusBottomRight;
        DrawRectangleRec(
            Rectangle{rec.x + rec.width - radiusTopRight, rightRectTop,
                      radiusTopRight, rightRectBottom - rightRectTop},
            color);
    }

    // Draw each corner (only if radius > 0)
    if (radiusBottomRight > 0) {
        DrawCorner(rec.x + rec.width - radiusBottomRight,
                   rec.y + rec.height - radiusBottomRight, radiusBottomRight,
                   segmentsBottomRight, color, 0.0f);
    }
    if (radiusBottomLeft > 0) {
        DrawCorner(rec.x + radiusBottomLeft,
                   rec.y + rec.height - radiusBottomLeft, radiusBottomLeft,
                   segmentsBottomLeft, color, 90.0f);
    }
    if (radiusTopRight > 0) {
        DrawCorner(rec.x + rec.width - radiusTopRight, rec.y + radiusTopRight,
                   radiusTopRight, segmentsTopRight, color, 270.0f);
    }
    if (radiusTopLeft > 0) {
        DrawCorner(rec.x + radiusTopLeft, rec.y + radiusTopLeft, radiusTopLeft,
                   segmentsTopLeft, color, 180.0f);
    }
}

}  // namespace raylib

namespace afterhours {

// Draw text with optional rotation support
// When rotation is non-zero, uses DrawTextPro to rotate around (centerX, centerY)
// When rotation is zero, uses standard DrawTextEx
inline void draw_text_ex(const raylib::Font font, const char *content,
                         const Vector2Type position, const float font_size,
                         const float spacing, const Color color,
                         const float rotation = 0.0f,
                         const float centerX = 0.0f, const float centerY = 0.0f) {
    if (std::abs(rotation) < 0.001f) {
        raylib::DrawTextEx(font, content, position, font_size, spacing, color);
        return;
    }
    // Use DrawTextPro for rotated text
    // Origin is the offset from position to the rotation center
    raylib::Vector2 origin = {centerX - position.x, centerY - position.y};
    raylib::Vector2 drawPos = {centerX, centerY};
    raylib::DrawTextPro(font, content, drawPos, origin, rotation, font_size, spacing, color);
}

inline void draw_text(const char *content, const float x, const float y,
                      const float font_size, const Color color) {
    raylib::DrawText(content, static_cast<int>(x), static_cast<int>(y),
                     static_cast<int>(font_size), color);
}

inline void draw_rectangle_outline(const RectangleType rect,
                                   const Color color) {
    raylib::DrawRectangleLinesEx(rect, 3.f, color);
}

inline void draw_rectangle_outline(const RectangleType rect, const Color color,
                                   const float thickness) {
    raylib::DrawRectangleLinesEx(rect, thickness, color);
}

inline void draw_rectangle(const RectangleType rect, const Color color) {
    raylib::DrawRectangleRec(rect, color);
}

inline void draw_rectangle_rounded(
    const RectangleType rect, const float roundness, const int segments,
    const Color color,
    const std::bitset<4> corners = std::bitset<4>().reset()) {
    if (corners.all()) {
        raylib::DrawRectangleRounded(rect, roundness, segments, color);
        return;
    }

    if (corners.none()) {
        draw_rectangle(rect, color);
        return;
    }

    const float ROUND = roundness;
    const float SHARP = 0.f;
    raylib::DrawRectangleCustom(
        rect,
        corners.test(3) ? ROUND : SHARP,  // Top-Left
        corners.test(2) ? ROUND : SHARP,  // Top Right
        corners.test(1) ? ROUND : SHARP,  // Bototm Left
        corners.test(0) ? ROUND : SHARP,  // Bottom Right
        segments, color);
}

inline void draw_rectangle_rounded_lines(
    const RectangleType rect, const float roundness, const int segments,
    const Color color,
    const std::bitset<4> corners = std::bitset<4>().reset()) {
    if (corners.all()) {
        raylib::DrawRectangleRoundedLines(rect, roundness, segments, color);
        return;
    }

    if (corners.none()) {
        draw_rectangle_outline(rect, color);
        return;
    }

    raylib::DrawRectangleRoundedLines(rect, roundness, segments, color);
}

// Draw a rotated rounded rectangle
// rotation: angle in degrees (clockwise)
// The rectangle rotates around its center
inline void draw_rectangle_rounded_rotated(
    const RectangleType rect, const float roundness, const int segments,
    const Color color, const std::bitset<4> corners, const float rotation) {
    // Skip rotation if angle is effectively zero
    if (std::abs(rotation) < 0.001f) {
        draw_rectangle_rounded(rect, roundness, segments, color, corners);
        return;
    }

    // Calculate center of the rectangle
    float centerX = rect.x + rect.width / 2.0f;
    float centerY = rect.y + rect.height / 2.0f;

    // Create a rect centered at origin
    RectangleType centeredRect = {
        -rect.width / 2.0f,
        -rect.height / 2.0f,
        rect.width,
        rect.height
    };

    // Apply transformation: translate to center, rotate, then draw
    raylib::rlPushMatrix();
    raylib::rlTranslatef(centerX, centerY, 0.0f);
    raylib::rlRotatef(rotation, 0.0f, 0.0f, 1.0f);

    // Draw the rectangle at origin (now rotated)
    draw_rectangle_rounded(centeredRect, roundness, segments, color, corners);

    raylib::rlPopMatrix();
}

// Draw a 9-slice (NPatch) texture stretched to fill a rectangle
inline void draw_texture_npatch(const raylib::Texture2D texture,
                                const RectangleType dest, int left, int top,
                                int right, int bottom,
                                const Color tint = Color{255, 255, 255, 255}) {
    raylib::NPatchInfo npatch_info = {
        .source =
            raylib::Rectangle{0.0f, 0.0f, static_cast<float>(texture.width),
                              static_cast<float>(texture.height)},
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
        .layout = raylib::NPATCH_NINE_PATCH};
    raylib::DrawTextureNPatch(texture, npatch_info, dest,
                              raylib::Vector2{0.0f, 0.0f}, 0.0f, tint);
}

// Draw a ring segment (arc with thickness)
inline void draw_ring_segment(float centerX, float centerY, float innerRadius,
                              float outerRadius, float startAngle,
                              float endAngle, int segments, Color color) {
    raylib::DrawRing(raylib::Vector2{centerX, centerY}, innerRadius,
                     outerRadius, startAngle, endAngle, segments, color);
}

// Draw a full ring (circle with hole)
inline void draw_ring(float centerX, float centerY, float innerRadius,
                      float outerRadius, int segments, Color color) {
    raylib::DrawRing(raylib::Vector2{centerX, centerY}, innerRadius,
                     outerRadius, 0.0f, 360.0f, segments, color);
}

// Begin scissor mode (clipping rectangle)
inline void begin_scissor_mode(int x, int y, int width, int height) {
    raylib::BeginScissorMode(x, y, width, height);
}

// End scissor mode (stop clipping)
inline void end_scissor_mode() { raylib::EndScissorMode(); }

// Push rotation transform
inline void push_rotation(float centerX, float centerY, float rotation) {
    raylib::rlPushMatrix();
    if (std::abs(rotation) >= 0.001f) {
        raylib::rlTranslatef(centerX, centerY, 0.0f);
        raylib::rlRotatef(rotation, 0.0f, 0.0f, 1.0f);
        raylib::rlTranslatef(-centerX, -centerY, 0.0f);
    }
}

// Pop rotation transform
inline void pop_rotation() {
    raylib::rlPopMatrix();
}

// Line drawing primitives
inline void draw_line(int x1, int y1, int x2, int y2, Color color) {
  raylib::DrawLine(x1, y1, x2, y2, color);
}

inline void draw_line_ex(Vector2Type start, Vector2Type end, float thickness,
                         Color color) {
  raylib::DrawLineEx(start, end, thickness, color);
}

inline void draw_line_strip(Vector2Type *points, int point_count, Color color) {
  raylib::DrawLineStrip(points, point_count, color);
}

// Circle drawing primitives
inline void draw_circle(int centerX, int centerY, float radius, Color color) {
  raylib::DrawCircle(centerX, centerY, radius, color);
}

inline void draw_circle_v(Vector2Type center, float radius, Color color) {
  raylib::DrawCircleV(center, radius, color);
}

inline void draw_circle_lines(int centerX, int centerY, float radius,
                              Color color) {
  raylib::DrawCircleLines(centerX, centerY, radius, color);
}

inline void draw_circle_sector(Vector2Type center, float radius,
                               float startAngle, float endAngle, int segments,
                               Color color) {
  raylib::DrawCircleSector(center, radius, startAngle, endAngle, segments,
                           color);
}

inline void draw_circle_sector_lines(Vector2Type center, float radius,
                                     float startAngle, float endAngle,
                                     int segments, Color color) {
  raylib::DrawCircleSectorLines(center, radius, startAngle, endAngle, segments,
                                color);
}

// Ellipse drawing primitives
inline void draw_ellipse(int centerX, int centerY, float radiusH, float radiusV,
                         Color color) {
  raylib::DrawEllipse(centerX, centerY, radiusH, radiusV, color);
}

inline void draw_ellipse_lines(int centerX, int centerY, float radiusH,
                               float radiusV, Color color) {
  raylib::DrawEllipseLines(centerX, centerY, radiusH, radiusV, color);
}

// Triangle drawing primitives
inline void draw_triangle(Vector2Type v1, Vector2Type v2, Vector2Type v3,
                          Color color) {
  raylib::DrawTriangle(v1, v2, v3, color);
}

inline void draw_triangle_lines(Vector2Type v1, Vector2Type v2, Vector2Type v3,
                                Color color) {
  raylib::DrawTriangleLines(v1, v2, v3, color);
}

// Polygon drawing primitives
inline void draw_poly(Vector2Type center, int sides, float radius, float rotation,
                      Color color) {
  raylib::DrawPoly(center, sides, radius, rotation, color);
}

inline void draw_poly_lines(Vector2Type center, int sides, float radius,
                            float rotation, Color color) {
  raylib::DrawPolyLines(center, sides, radius, rotation, color);
}

inline void draw_poly_lines_ex(Vector2Type center, int sides, float radius,
                               float rotation, float lineThick, Color color) {
  raylib::DrawPolyLinesEx(center, sides, radius, rotation, lineThick, color);
}

inline void set_mouse_cursor(int cursor_id) {
  raylib::SetMouseCursor(cursor_id);
}

inline raylib::Font get_default_font() { return raylib::GetFontDefault(); }
inline raylib::Font get_unset_font() { return raylib::GetFontDefault(); }

}  // namespace afterhours
