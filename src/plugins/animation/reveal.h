#pragma once

#include <algorithm>

#include "instant.h"
#include "timeline.h"

namespace afterhours {
namespace motion {

struct Reveal {
  float duration = 0.2f;

  float (*curve)(float) = &curves::ease_in_out_quad;

  float progress = 0.f;

  float update(float dt, bool open) {
    const float target = open ? 1.f : 0.f;
    if (is_instant() || duration <= 0.f) {
      progress = target;
      return progress;
    }
    const float step = dt / duration;
    if (step <= 0.f)
      return progress;
    progress = std::clamp(progress + (open ? step : -step), 0.f, 1.f);
    return progress;
  }

  [[nodiscard]] float eased() const {
    if (progress <= 0.f || progress >= 1.f)
      return progress;
    return curve ? curve(progress) : progress;
  }

  [[nodiscard]] bool settled(bool open) const {
    return progress == (open ? 1.f : 0.f);
  }

  void snap(bool open) { progress = open ? 1.f : 0.f; }
};

} // namespace motion
} // namespace afterhours
