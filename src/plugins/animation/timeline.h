#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace afterhours {
namespace motion {

namespace curves {
inline float ease_in_quad(float u) { return u * u; }
inline float ease_out_quad(float u) { return 1.f - (1.f - u) * (1.f - u); }
inline float ease_out_cubic(float u) {
  const float r = 1.f - u;
  return 1.f - r * r * r;
}
inline float ease_in_out_quad(float u) {
  return u < 0.5f ? 2.f * u * u : 1.f - (-2.f * u + 2.f) * (-2.f * u + 2.f) / 2.f;
}
} // namespace curves

struct Timeline {
  struct Key {
    float t = 0.f;
    float v = 0.f;
  };
  enum struct Repeat { Once, Loop, PingPong };
  std::vector<Key> keys;
  Repeat repeat = Repeat::Once;
  float (*curve)(float) = nullptr;

  float length() const { return keys.empty() ? 0.f : keys.back().t; }
  bool finished(float t) const {
    return repeat == Repeat::Once && t >= length();
  }
  float at(float t) const {
    if (keys.empty())
      return 0.f;
    const float len = length();
    if (len <= 0.f)
      return keys.back().v;
    t = std::max(t, 0.f);
    switch (repeat) {
    case Repeat::Once:
      t = std::min(t, len);
      break;
    case Repeat::Loop:
      t = std::fmod(t, len);
      break;
    case Repeat::PingPong: {
      const float cycle = std::fmod(t, 2.f * len);
      t = cycle <= len ? cycle : 2.f * len - cycle;
      break;
    }
    }
    if (t <= keys.front().t)
      return keys.front().v;
    const auto next = std::upper_bound(
        keys.begin(), keys.end(), t,
        [](float value, const Key &k) { return value < k.t; });
    if (next == keys.end())
      return keys.back().v;
    const Key &a = *(next - 1);
    const Key &b = *next;
    const float span = b.t - a.t;
    float u = span > 0.f ? (t - a.t) / span : 1.f;
    if (curve)
      u = curve(u);
    return std::lerp(a.v, b.v, u);
  }
};

// Instant mode: every animation lands on its final value on the first update.
//
// Deliberately not an enabled/disabled flag. The animation code still runs and
// on_complete still fires, so nothing downstream has to branch on it -- which
// is what makes it safe to leave on. Three callers want the same knob: e2e
// wants screenshots of the settled state, accessibility wants reduce-motion,
// and dev iteration wants to skip the wait.

} // namespace motion
} // namespace afterhours
