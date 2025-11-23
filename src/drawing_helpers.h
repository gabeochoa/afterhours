
#pragma once

#include "developer.h"
#include "font_helper.h"
#include "plugins/color.h"

#ifdef AFTER_HOURS_USE_RAYLIB
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
#endif

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB

inline void draw_text_ex(const raylib::Font font, const char *content,
                         const Vector2Type position, const float font_size,
                         const float spacing, const Color color) {
    raylib::DrawTextEx(font, content, position, font_size, spacing, color);
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

inline raylib::Font get_default_font() { return raylib::GetFontDefault(); }
inline raylib::Font get_unset_font() { return raylib::GetFontDefault(); }

#else
inline void draw_text_ex(const afterhours::Font, const char *,
                         const Vector2Type, const float, const float,
                         const Color) {}
inline void draw_text(const char *, const float, const float, const float,
                      const Color) {}
inline void draw_rectangle(const RectangleType, const Color) {}
inline void draw_rectangle_outline(const RectangleType, const Color) {}
inline void draw_rectangle_outline(const RectangleType, const Color,
                                   const float) {}
inline void draw_rectangle_rounded(const RectangleType, const float, const int,
                                   const Color, const std::bitset<4>) {}
inline void draw_rectangle_rounded_lines(const RectangleType, const float,
                                         const int, const Color,
                                         const std::bitset<4>) {}
inline afterhours::Font get_default_font() { return afterhours::Font(); }
inline afterhours::Font get_unset_font() { return afterhours::Font(); }
#endif

}  // namespace afterhours
