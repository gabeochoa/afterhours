#pragma once

// Reveal open/close progress for panels and sections (R-211).
//
//   Reveal r;                 // one per panel, e.g. a component member
//   float p = r.update(dt, open);
//   // size/clip/or fade by r.eased(), keep building while !r.settled()
//
// on_appear only animates entry, on_state animates a spring target with no
// caller-visible progress, and the disclosure draft (#239) is binary with no
// animation. Reveal is the widget-level sibling of presentation::host
// (#354, screen-level present/dismiss): a 0..1 progress driven by an open
// bool over `duration`, with easing from motion::curves and instant/reduced
// motion snapping. Toggling mid-flight reverses from the current value.

#include <algorithm>

#include "animation/instant.h"
#include "animation/timeline.h"

namespace afterhours {

struct Reveal {
  float duration = 0.2f;
  float (*curve)(float) = &motion::curves::ease_in_out_quad;
  float progress = 0.f;

  // Advance toward `open` by dt seconds; returns raw progress (0 closed,
  // 1 open). Progress clamps at both ends and dt <= 0 changes nothing.
  float update(float dt, bool open) {
    const float target = open ? 1.f : 0.f;
    if (motion::is_instant() || duration <= 0.f) {
      progress = target;
      return progress;
    }
    const float step = dt / duration;
    if (step <= 0.f)
      return progress;
    progress = std::clamp(progress + (open ? step : -step), 0.f, 1.f);
    return progress;
  }

  // Eased progress: size/clip/fade by this. Ends stay exact so a fully
  // open/closed panel lands precisely.
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

} // namespace afterhours
