#pragma once

#include <algorithm>
#include <cmath>

#include "../developer.h"

namespace afterhours {
namespace math {

inline float smoothstep(float edge0, float edge1, float x) {
  if (edge0 == edge1)
    return x < edge0 ? 0.0f : 1.0f;
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

inline float damp(float current, float target, float lambda, float dt) {
  return current + (target - current) * (1.0f - std::exp(-lambda * dt));
}

inline float wrap_angle(float rad) {
  const float two_pi = 2.0f * PI;
  float wrapped = std::fmod(rad + PI, two_pi);
  if (wrapped < 0.0f)
    wrapped += two_pi;
  return wrapped - PI;
}

inline float lerp_angle(float a, float b, float t) {
  return a + wrap_angle(b - a) * t;
}

} // namespace math
} // namespace afterhours
