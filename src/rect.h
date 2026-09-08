#pragma once

// Rectangle algebra: the arithmetic a render pass does after layout hands back
// a rect. Free functions, because RectangleType is a #define and cannot take
// members.
//
// This is not the imm box model. It is `take a 22px header off this panel,
// inset the rest by 6, centre a 64x36 box in it`, which previously needed a
// geometry header in every consumer.

// ── Box model (read this if pad and expand feel backwards) ─────────────────
// The rect you pass in is the border box, the thing you actually draw:
//
//     ┌──────────────── expand ───────────────────┐   edges move OUT, bigger
//     │    ┌──────── the rect you pass ────────┐  │
//     │    │    ┌──────── pad ────────────┐    │  │   edges move IN, smaller
//     │    │    │        content          │    │  │
//     │    │    └─────────────────────────┘    │  │
//     │    └───────────────────────────────────┘  │
//     └───────────────────────────────────────────┘
//
// A hit target uses expand: it is bigger than the visual. A screen inside a
// bezel uses pad: it is inside the frame.

#include <algorithm>

#include "developer.h"

namespace afterhours {
namespace rect {

// NOTE: {top, left, bottom, right}, matching Padding and Margin. That is NOT
// CSS order -- puzzle's original is {top, right, bottom, left} -- so a file
// moved here verbatim will silently swap left and right. Designated
// initialisers (`{.top = 4}`) are immune and are the recommended spelling.
struct Sides {
  float top = 0.f;
  float left = 0.f;
  float bottom = 0.f;
  float right = 0.f;

  static constexpr Sides all(float v) { return {v, v, v, v}; }
  static constexpr Sides vertical(float v) { return {v, 0.f, v, 0.f}; }
  static constexpr Sides horizontal(float v) { return {0.f, v, 0.f, v}; }
};

// A strip and what is left, always in spatial order: for cut_top the strip is
// the upper piece, for cut_left the left one. Structured-binding friendly.
struct Split {
  RectangleType first;
  RectangleType second;
};

// Never negative: a pad bigger than the rect gives an empty rect at the
// expected corner rather than one that draws inside out.
inline RectangleType pad(RectangleType r, Sides s) {
  const float w = std::max(0.f, r.width - s.left - s.right);
  const float h = std::max(0.f, r.height - s.top - s.bottom);
  return RectangleType{r.x + s.left, r.y + s.top, w, h};
}
inline RectangleType pad(RectangleType r, float all) {
  return pad(r, Sides::all(all));
}

inline RectangleType expand(RectangleType r, Sides s) {
  return RectangleType{r.x - s.left, r.y - s.top, r.width + s.left + s.right,
                       r.height + s.top + s.bottom};
}
inline RectangleType expand(RectangleType r, float all) {
  return expand(r, Sides::all(all));
}

// Cuts clamp, so cutting more than there is leaves an empty remainder rather
// than a negative one.
inline Split cut_top(RectangleType r, float h) {
  const float take = std::clamp(h, 0.f, r.height);
  return {RectangleType{r.x, r.y, r.width, take},
          RectangleType{r.x, r.y + take, r.width, r.height - take}};
}
inline Split cut_bottom(RectangleType r, float h) {
  const float take = std::clamp(h, 0.f, r.height);
  return {RectangleType{r.x, r.y, r.width, r.height - take},
          RectangleType{r.x, r.y + r.height - take, r.width, take}};
}
inline Split cut_left(RectangleType r, float w) {
  const float take = std::clamp(w, 0.f, r.width);
  return {RectangleType{r.x, r.y, take, r.height},
          RectangleType{r.x + take, r.y, r.width - take, r.height}};
}
inline Split cut_right(RectangleType r, float w) {
  const float take = std::clamp(w, 0.f, r.width);
  return {RectangleType{r.x, r.y, r.width - take, r.height},
          RectangleType{r.x + r.width - take, r.y, take, r.height}};
}

inline RectangleType align_center(RectangleType r, float w, float h) {
  return RectangleType{r.x + (r.width - w) * 0.5f, r.y + (r.height - h) * 0.5f,
                       w, h};
}
inline RectangleType align_top(RectangleType r, float w, float h) {
  return RectangleType{r.x + (r.width - w) * 0.5f, r.y, w, h};
}
inline RectangleType align_bottom(RectangleType r, float w, float h) {
  return RectangleType{r.x + (r.width - w) * 0.5f, r.y + r.height - h, w, h};
}
inline RectangleType align_left(RectangleType r, float w, float h) {
  return RectangleType{r.x, r.y + (r.height - h) * 0.5f, w, h};
}
inline RectangleType align_right(RectangleType r, float w, float h) {
  return RectangleType{r.x + r.width - w, r.y + (r.height - h) * 0.5f, w, h};
}

inline RectangleType with_size(RectangleType r, float w, float h) {
  return RectangleType{r.x, r.y, w, h};
}
inline RectangleType with_pos(RectangleType r, float x, float y) {
  return RectangleType{x, y, r.width, r.height};
}
inline RectangleType offset(RectangleType r, float dx, float dy) {
  return RectangleType{r.x + dx, r.y + dy, r.width, r.height};
}

inline Vector2Type center(RectangleType r) {
  return Vector2Type{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

inline bool contains(RectangleType r, Vector2Type p) {
  return p.x >= r.x && p.y >= r.y && p.x <= r.x + r.width &&
         p.y <= r.y + r.height;
}

// Touching edges do not overlap: two rects from the same cut share an edge and
// are not on top of each other.
inline bool overlaps(RectangleType a, RectangleType b) {
  return a.x < b.x + b.width && b.x < a.x + a.width &&
         a.y < b.y + b.height && b.y < a.y + a.height;
}

} // namespace rect
} // namespace afterhours
