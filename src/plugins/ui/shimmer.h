#pragma once

#include "../../drawing_helpers.h"
#include "../animation_presets/effect_presets.h"

namespace afterhours {

inline void draw_shimmer(const RectangleType rect, const float t,
                         const presets::Shimmer &s) {
  int count = 0;
  const auto strips = presets::shimmer_strips(s, rect, t, count);
  for (int i = 0; i < count; ++i)
    draw_rectangle(strips[i].rect, strips[i].color);
}

}
