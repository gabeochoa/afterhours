#pragma once

#include "../../drawing_helpers.h"
#include "../animation_presets/effect_presets.h"

namespace afterhours {

// Draws a shimmer sweep over rect at time t (seconds). The band math lives
// in presets::shimmer_strips; this is only the draw loop, and it works on
// every backend because it is built on draw_rectangle alone.
inline void draw_shimmer(const RectangleType rect, const float t,
                         const presets::Shimmer &s) {
  int count = 0;
  const auto strips = presets::shimmer_strips(s, rect, t, count);
  for (int i = 0; i < count; ++i)
    draw_rectangle(strips[i].rect, strips[i].color);
}

} // namespace afterhours
