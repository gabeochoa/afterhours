#pragma once

// Anchored overlay placement: where a dropdown, context menu or popover should
// sit relative to the thing that opened it. Pure geometry, no entities, so the
// three widgets share one implementation of flipping and clamping.

#include <algorithm>

#include "../../developer.h"

namespace afterhours {
namespace ui {
namespace overlay {

enum struct Placement { Below, Above, Right, Left };

struct Placed {
  float x = 0.f;
  float y = 0.f;
  Placement used = Placement::Below;
  // True when the preferred side did not fit and the opposite one was taken.
  bool flipped = false;
};

inline Placement opposite_of(Placement p) {
  switch (p) {
  case Placement::Below:
    return Placement::Above;
  case Placement::Above:
    return Placement::Below;
  case Placement::Right:
    return Placement::Left;
  case Placement::Left:
    return Placement::Right;
  }
  return Placement::Below;
}

namespace detail {
// Top-left corner for a side, before any clamping.
inline void corner_for(Placement p, const RectangleType &anchor, float w,
                       float h, float gap, float &out_x, float &out_y) {
  switch (p) {
  case Placement::Below:
    out_x = anchor.x;
    out_y = anchor.y + anchor.height + gap;
    return;
  case Placement::Above:
    out_x = anchor.x;
    out_y = anchor.y - h - gap;
    return;
  case Placement::Right:
    out_x = anchor.x + anchor.width + gap;
    out_y = anchor.y;
    return;
  case Placement::Left:
    out_x = anchor.x - w - gap;
    out_y = anchor.y;
    return;
  }
  out_x = anchor.x;
  out_y = anchor.y;
}

// Room between the anchor and the screen edge on a given side.
inline float room_for(Placement p, const RectangleType &anchor,
                      float screen_w, float screen_h, float gap) {
  switch (p) {
  case Placement::Below:
    return screen_h - (anchor.y + anchor.height + gap);
  case Placement::Above:
    return anchor.y - gap;
  case Placement::Right:
    return screen_w - (anchor.x + anchor.width + gap);
  case Placement::Left:
    return anchor.x - gap;
  }
  return 0.f;
}
} // namespace detail

// Place a `w` x `h` overlay against `anchor` on the preferred side, flipping to
// the opposite side when it would run off screen and clamping so it stays
// visible either way. When neither side fits, the roomier one wins -- clamped
// content beats content placed off screen.
inline Placed place(const RectangleType &anchor, float w, float h,
                    float screen_w, float screen_h,
                    Placement preferred = Placement::Below, float gap = 0.f) {
  const bool vertical =
      preferred == Placement::Below || preferred == Placement::Above;
  const float needed = vertical ? h : w;

  Placement chosen = preferred;
  bool flipped = false;
  const float room = detail::room_for(preferred, anchor, screen_w, screen_h, gap);
  if (room < needed) {
    const Placement other = opposite_of(preferred);
    const float other_room =
        detail::room_for(other, anchor, screen_w, screen_h, gap);
    if (other_room > room) {
      chosen = other;
      flipped = true;
    }
  }

  Placed out;
  detail::corner_for(chosen, anchor, w, h, gap, out.x, out.y);
  out.used = chosen;
  out.flipped = flipped;

  // Clamp last: a flip fixes the main axis, this keeps the cross axis (and an
  // overlay too big for either side) on screen.
  out.x = std::clamp(out.x, 0.f, std::max(0.f, screen_w - w));
  out.y = std::clamp(out.y, 0.f, std::max(0.f, screen_h - h));
  return out;
}

} // namespace overlay
} // namespace ui
} // namespace afterhours
