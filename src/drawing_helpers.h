
#pragma once

#include "developer.h"
#include "font_helper.h"
#include "plugins/color.h"

#ifdef AFTER_HOURS_USE_RAYLIB
namespace raylib {
#define SMOOTH_CIRCLE_ERROR_RATE 0.5f
static Texture2D texShapes = {1, 1, 1, 1, 7};

// AI
inline int CalculateSegments(float radius) {
  float th = acosf(2 * powf(1 - SMOOTH_CIRCLE_ERROR_RATE / radius, 2) - 1);
  return (int)(ceilf(2 * PI / th) / 4.0f);
}

inline void DrawCorner(float x, float y, float radius, int segments,
                       Color color, float angle) {
  float stepLength = 90.0f / (float)segments;

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

/*
inline void DrawRectangleCustom(Rectangle rec, float roundnessTopLeft,
                                float roundnessTopRight,
                                float roundnessBottomRight,
                                float roundnessBottomLeft, int segments,
                                Color color) {
  // Calculate corner radius for each corner
  float radiusTopLeft = (rec.width > rec.height)
                            ? (rec.height * roundnessTopLeft) / 2
                            : (rec.width * roundnessTopLeft) / 2;
  float radiusTopRight = (rec.width > rec.height)
                             ? (rec.height * roundnessTopRight) / 2
                             : (rec.width * roundnessTopRight) / 2;
  float radiusBottomRight = (rec.width > rec.height)
                                ? (rec.height * roundnessBottomRight) / 2
                                : (rec.width * roundnessBottomRight) / 2;
  float radiusBottomLeft = (rec.width > rec.height)
                               ? (rec.height * roundnessBottomLeft) / 2
                               : (rec.width * roundnessBottomLeft) / 2;

  // Calculate number of segments to use for each corner
  int segmentsTopLeft =
      (segments < 4) ? CalculateSegments(radiusTopLeft) : segments;
  int segmentsTopRight =
      (segments < 4) ? CalculateSegments(radiusTopRight) : segments;
  int segmentsBottomRight =
      (segments < 4) ? CalculateSegments(radiusBottomRight) : segments;
  int segmentsBottomLeft =
      (segments < 4) ? CalculateSegments(radiusBottomLeft) : segments;

  float stepLengthTopLeft = 90.0f / (float)segmentsTopLeft;
  float stepLengthTopRight = 90.0f / (float)segmentsTopRight;
  float stepLengthBottomRight = 90.0f / (float)segmentsBottomRight;
  float stepLengthBottomLeft = 90.0f / (float)segmentsBottomLeft;

  const Vector2 point[12] = {
      {(float)rec.x + radiusTopLeft, rec.y},
      {(float)(rec.x + rec.width) - radiusTopRight, rec.y},
      {rec.x + rec.width, (float)rec.y + radiusTopRight}, // PO, P1, P2
      {rec.x + rec.width, (float)(rec.y + rec.height) - radiusBottomRight},
      {(float)(rec.x + rec.width) - radiusBottomRight,
       rec.y + rec.height}, // P3, P4
      {(float)rec.x + radiusBottomLeft, rec.y + rec.height},
      {rec.x, (float)(rec.y + rec.height) - radiusBottomLeft},
      {rec.x, (float)rec.y + radiusTopLeft}, // P5, P6, P7
      {(float)rec.x + radiusTopLeft, (float)rec.y + radiusTopLeft},
      {(float)(rec.x + rec.width) - radiusTopRight,
       (float)rec.y + radiusTopRight}, // P8, P9
      {(float)(rec.x + rec.width) - radiusBottomRight,
       (float)(rec.y + rec.height) - radiusBottomRight},
      {(float)rec.x + radiusBottomLeft,
       (float)(rec.y + rec.height) - radiusBottomLeft} // P10, P11
  };

  const Vector2 centers[4] = {point[8], point[9], point[10], point[11]};
  const float angles[4] = {180.0f, 270.0f, 0.0f, 90.0f};

  rlBegin(RL_TRIANGLES);
  // Draw all the 4 corners
  for (int k = 0; k < 4; ++k) {
    float angle = angles[k];
    const Vector2 center = centers[k];
    float stepLength = 0.0f;
    float radius = 0.0f;

    switch (k) {
    case 0:
      stepLength = stepLengthTopLeft;
      radius = radiusTopLeft;
      break;
    case 1:
      stepLength = stepLengthTopRight;
      radius = radiusTopRight;
      break;
    case 2:
      stepLength = stepLengthBottomRight;
      radius = radiusBottomRight;
      break;
    case 3:
      stepLength = stepLengthBottomLeft;
      radius = radiusBottomLeft;
      break;
    }

    for (int i = 0; i < segments; i++) {
      rlColor4ub(color.r, color.g, color.b, color.a);
      rlVertex2f(center.x, center.y);
      rlVertex2f(center.x + cosf(DEG2RAD * (angle + stepLength)) * radius,
                 center.y + sinf(DEG2RAD * (angle + stepLength)) * radius);
      rlVertex2f(center.x + cosf(DEG2RAD * angle) * radius,
                 center.y + sinf(DEG2RAD * angle) * radius);

      angle += stepLength;
    }
  }

  // [2] Upper Rectangle
  rlColor4ub(color.r, color.g, color.b, color.a);
  rlVertex2f(point[0].x, point[0].y);
  rlVertex2f(point[8].x, point[8].y);
  rlVertex2f(point[9].x, point[9].y);

  rlVertex2f(point[0].x, point[0].y);
  rlVertex2f(point[9].x, point[9].y);
  rlVertex2f(point[1].x, point[1].y);

  // [4] Right Rectangle
  rlColor4ub(color.r, color.g, color.b, color.a);
  rlVertex2f(point[2].x, point[2].y);
  rlVertex2f(point[9].x, point[9].y);
  rlVertex2f(point[10].x, point[10].y);

  rlVertex2f(point[2].x, point[2].y);
  rlVertex2f(point[10].x, point[10].y);
  rlVertex2f(point[3].x, point[3].y);

  // [6] Bottom Rectangle
  rlColor4ub(color.r, color.g, color.b, color.a);
  rlVertex2f(point[11].x, point[11].y);
  rlVertex2f(point[5].x, point[5].y);
  rlVertex2f(point[4].x, point[4].y);

  rlVertex2f(point[11].x, point[11].y);
  rlVertex2f(point[4].x, point[4].y);
  rlVertex2f(point[10].x, point[10].y);

  // [8] Left Rectangle
  rlColor4ub(color.r, color.g, color.b, color.a);
  rlVertex2f(point[7].x, point[7].y);
  rlVertex2f(point[6].x, point[6].y);
  rlVertex2f(point[11].x, point[11].y);

  rlVertex2f(point[7].x, point[7].y);
  rlVertex2f(point[11].x, point[11].y);
  rlVertex2f(point[8].x, point[8].y);

  rlEnd();
}

*/
/*
inline void DrawRectangleCustom(Rectangle rec, float roundnessTopLeft,
    float roundnessTopRight,
    float roundnessBottomRight,
    float roundnessBottomLeft, int segments,
    Color color) {
// Calculate corner radius for each corner
float radiusTopLeft = (rec.width > rec.height)
? (rec.height * roundnessTopLeft) / 2
: (rec.width * roundnessTopLeft) / 2;
float radiusTopRight = (rec.width > rec.height)
 ? (rec.height * roundnessTopRight) / 2
 : (rec.width * roundnessTopRight) / 2;
float radiusBottomRight = (rec.width > rec.height)
    ? (rec.height * roundnessBottomRight) / 2
    : (rec.width * roundnessBottomRight) / 2;
float radiusBottomLeft = (rec.width > rec.height)
   ? (rec.height * roundnessBottomLeft) / 2
   : (rec.width * roundnessBottomLeft) / 2;

// Calculate number of segments to use for each corner
int segmentsTopLeft =
(segments < 4) ? CalculateSegments(radiusTopLeft) : segments;
int segmentsTopRight =
(segments < 4) ? CalculateSegments(radiusTopRight) : segments;
int segmentsBottomRight =
(segments < 4) ? CalculateSegments(radiusBottomRight) : segments;
int segmentsBottomLeft =
(segments < 4) ? CalculateSegments(radiusBottomLeft) : segments;

float stepLengthTopLeft = 90.0f / (float)segmentsTopLeft;
float stepLengthTopRight = 90.0f / (float)segmentsTopRight;
float stepLengthBottomRight = 90.0f / (float)segmentsBottomRight;
float stepLengthBottomLeft = 90.0f / (float)segmentsBottomLeft;

const Vector2 point[12] = {
{(float)rec.x + radiusTopLeft, rec.y},
{(float)(rec.x + rec.width) - radiusTopRight, rec.y},
{rec.x + rec.width, (float)rec.y + radiusTopRight}, // PO, P1, P2
{rec.x + rec.width, (float)(rec.y + rec.height) - radiusBottomRight},
{(float)(rec.x + rec.width) - radiusBottomRight,
rec.y + rec.height}, // P3, P4
{(float)rec.x + radiusBottomLeft, rec.y + rec.height},
{rec.x, (float)(rec.y + rec.height) - radiusBottomLeft},
{rec.x, (float)rec.y + radiusTopLeft}, // P5, P6, P7
{(float)rec.x + radiusTopLeft, (float)rec.y + radiusTopLeft},
{(float)(rec.x + rec.width) - radiusTopRight,
(float)rec.y + radiusTopRight}, // P8, P9
{(float)(rec.x + rec.width) - radiusBottomRight,
(float)(rec.y + rec.height) - radiusBottomRight},
{(float)rec.x + radiusBottomLeft,
(float)(rec.y + rec.height) - radiusBottomLeft} // P10, P11
};

const Vector2 centers[4] = {point[8], point[9], point[10], point[11]};
const float angles[4] = {180.0f, 270.0f, 0.0f, 90.0f};

rlSetTexture(GetShapesTexture().id);
Rectangle shapeRect = GetShapesTextureRectangle();

rlBegin(RL_QUADS);
// Draw all the 4 corners
for (int k = 0; k < 4; ++k) {
float angle = angles[k];
const Vector2 center = centers[k];
float stepLength = 0.0f;
float radius = 0.0f;

switch (k) {
case 0:
stepLength = stepLengthTopLeft;
radius = radiusTopLeft;
break;
case 1:
stepLength = stepLengthTopRight;
radius = radiusTopRight;
break;
case 2:
stepLength = stepLengthBottomRight;
radius = radiusBottomRight;
break;
case 3:
stepLength = stepLengthBottomLeft;
radius = radiusBottomLeft;
break;
}

// NOTE: Every QUAD actually represents two segments
for (int i = 0; i < segments / 2; i++) {
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(center.x, center.y);

rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(center.x + cosf(DEG2RAD * (angle + stepLength * 2)) * radius,
center.y + sinf(DEG2RAD * (angle + stepLength * 2)) * radius);

rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(center.x + cosf(DEG2RAD * (angle + stepLength)) * radius,
center.y + sinf(DEG2RAD * (angle + stepLength)) * radius);

rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(center.x + cosf(DEG2RAD * angle) * radius,
center.y + sinf(DEG2RAD * angle) * radius);

angle += (stepLength * 2);
}

// NOTE: In case number of segments is odd, we add one last piece to the
// cake
if (segments % 2) {
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(center.x, center.y);

rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(center.x + cosf(DEG2RAD * (angle + stepLength)) * radius,
center.y + sinf(DEG2RAD * (angle + stepLength)) * radius);

rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(center.x + cosf(DEG2RAD * angle) * radius,
center.y + sinf(DEG2RAD * angle) * radius);

rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(center.x, center.y);
}
}

// [2] Upper Rectangle
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width, shapeRect.y / texShapes.height);
rlVertex2f(point[0].x, point[0].y);
rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[8].x, point[8].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[9].x, point[9].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(point[1].x, point[1].y);
// [4] Right Rectangle
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width, shapeRect.y / texShapes.height);
rlVertex2f(point[2].x, point[2].y);
rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[9].x, point[9].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[10].x, point[10].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(point[3].x, point[3].y);

// [6] Bottom Rectangle
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width, shapeRect.y / texShapes.height);
rlVertex2f(point[11].x, point[11].y);
rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[5].x, point[5].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[4].x, point[4].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(point[10].x, point[10].y);

// [8] Left Rectangle
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width, shapeRect.y / texShapes.height);
rlVertex2f(point[7].x, point[7].y);
rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[6].x, point[6].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[11].x, point[11].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(point[8].x, point[8].y);

// [9] Middle Rectangle
rlColor4ub(color.r, color.g, color.b, color.a);
rlTexCoord2f(shapeRect.x / texShapes.width, shapeRect.y / texShapes.height);
rlVertex2f(point[8].x, point[8].y);
rlTexCoord2f(shapeRect.x / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[11].x, point[11].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
(shapeRect.y + shapeRect.height) / texShapes.height);
rlVertex2f(point[10].x, point[10].y);
rlTexCoord2f((shapeRect.x + shapeRect.width) / texShapes.width,
shapeRect.y / texShapes.height);
rlVertex2f(point[9].x, point[9].y);

rlEnd();
rlSetTexture(0);
}
*/

inline void DrawRectangleCustom(Rectangle rec, float roundnessTopLeft,
                                float roundnessTopRight,
                                float roundnessBottomLeft,
                                float roundnessBottomRight, int segments,
                                Color color) {
  // Calculate corner radius for each corner
  float radiusTopLeft = (rec.width > rec.height)
                            ? (rec.height * roundnessTopLeft) / 2
                            : (rec.width * roundnessTopLeft) / 2;
  float radiusTopRight = (rec.width > rec.height)
                             ? (rec.height * roundnessTopRight) / 2
                             : (rec.width * roundnessTopRight) / 2;
  float radiusBottomRight = (rec.width > rec.height)
                                ? (rec.height * roundnessBottomRight) / 2
                                : (rec.width * roundnessBottomRight) / 2;
  float radiusBottomLeft = (rec.width > rec.height)
                               ? (rec.height * roundnessBottomLeft) / 2
                               : (rec.width * roundnessBottomLeft) / 2;

  // Calculate number of segments to use for each corner
  int segmentsTopLeft =
      (segments < 4) ? CalculateSegments(radiusTopLeft) : segments;
  int segmentsTopRight =
      (segments < 4) ? CalculateSegments(radiusTopRight) : segments;
  int segmentsBottomRight =
      (segments < 4) ? CalculateSegments(radiusBottomRight) : segments;
  int segmentsBottomLeft =
      (segments < 4) ? CalculateSegments(radiusBottomLeft) : segments;

  // Draw each corner
  DrawCorner(rec.x + radiusTopLeft, rec.y + radiusTopLeft, radiusTopLeft,
             segmentsTopLeft, color, 180.0f);
  DrawCorner(rec.x + rec.width - radiusTopRight, rec.y + radiusTopRight,
             radiusTopRight, segmentsTopRight, color, 270.0f);
  DrawCorner(rec.x + radiusBottomLeft, rec.y + rec.height - radiusBottomLeft,
             radiusBottomLeft, segmentsBottomLeft, color, 90.0f);
  DrawCorner(rec.x + rec.width - radiusBottomRight,
             rec.y + rec.height - radiusBottomRight, radiusBottomRight,
             segmentsBottomRight, color, 0.0f);

  // Draw rectangles for each side
  DrawRectangleRec(Rectangle{rec.x + radiusTopLeft, rec.y,
                             rec.width - radiusTopLeft - radiusTopRight,
                             rec.height},
                   color);
  DrawRectangleRec(Rectangle{rec.x, rec.y + radiusTopLeft, rec.width,
                             rec.height - radiusTopLeft - radiusBottomLeft},
                   color);
}

} // namespace raylib
#endif

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
  // if (corners.all()) {
  // raylib::DrawRectangleRounded(rect, roundness, segments, color);
  // return;
  // }

  // if (corners.none()) {
  // draw_rectangle(rect, color);
  // return;
  // }

  const float ROUND = roundness;
  const float SHARP = 0.f;
  raylib::DrawRectangleCustom(rect,
                              corners.test(0) ? ROUND : SHARP, //
                              corners.test(1) ? ROUND : SHARP, //
                              corners.test(2) ? ROUND : SHARP, //
                              corners.test(3) ? ROUND : SHARP, //
                              segments, color);
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
