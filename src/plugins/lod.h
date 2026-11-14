#pragma once

// Level of detail from distance, without flicker at the boundaries.
//
//   Lod lod;  // one per copy, e.g. a member of its component
//   int level = lod.update(distance, {25.f, 60.f});  // 0 near, 1, then 2
//   int level = lod.update_sq(distance_sq, {25.f, 60.f});  // same, squared in
//
// Each threshold is where the next level starts. A copy only moves out a
// level once it is `margin` past the threshold, and only moves back in once
// it is `margin` inside it, so one hovering on the line does not switch every
// frame. cartographer's switch at 35m going out and 25m coming back is a
// threshold of 30 with a margin of about 0.17.

#include <initializer_list>
#include <span>

namespace afterhours {

struct Lod {
  int level = 0;

  int update(float distance, std::span<const float> thresholds,
             float margin = 0.15f) {
    int n = static_cast<int>(thresholds.size());
    if (level > n) level = n; // fewer thresholds than last time
    auto at = [&](int i) { return thresholds[static_cast<size_t>(i)]; };
    while (level < n && distance > at(level) * (1.f + margin)) level++;
    while (level > 0 && distance < at(level - 1) * (1.f - margin)) level--;
    return level;
  }

  int update(float distance, std::initializer_list<float> thresholds,
             float margin = 0.15f) {
    return update(distance,
                  std::span<const float>(thresholds.begin(), thresholds.size()),
                  margin);
  }

  // Same as update, for a loop that already has squared distance: the
  // thresholds stay linear and are squared here, so no sqrt per copy.
  int update_sq(float distance_sq, std::span<const float> thresholds,
                float margin = 0.15f) {
    int n = static_cast<int>(thresholds.size());
    if (level > n) level = n;
    auto sq = [](float v) { return v * v; };
    auto at = [&](int i) { return thresholds[static_cast<size_t>(i)]; };
    while (level < n && distance_sq > sq(at(level) * (1.f + margin))) level++;
    while (level > 0 && distance_sq < sq(at(level - 1) * (1.f - margin)))
      level--;
    return level;
  }

  int update_sq(float distance_sq, std::initializer_list<float> thresholds,
                float margin = 0.15f) {
    return update_sq(
        distance_sq,
        std::span<const float>(thresholds.begin(), thresholds.size()), margin);
  }
};

} // namespace afterhours
