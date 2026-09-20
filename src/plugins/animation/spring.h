#pragma once

#include <algorithm>
#include <cmath>

#ifndef AFTERHOURS_SPRING_CRITICAL_ZETA
#define AFTERHOURS_SPRING_CRITICAL_ZETA 0.9999f
#endif
#ifndef AFTERHOURS_SPRING_REST_RATIO
#define AFTERHOURS_SPRING_REST_RATIO 1e-3f
#endif

namespace afterhours {
namespace motion {

inline constexpr float kCriticalZeta = AFTERHOURS_SPRING_CRITICAL_ZETA;
inline constexpr float kRestRatio = AFTERHOURS_SPRING_REST_RATIO;

struct Spring {
  float response = 0.3f;
  float bounce = 0.f;
  float rest_delta = 0.f;
  float rest_speed = 0.f;

  static Spring smooth() { return {.response = 0.3f, .bounce = 0.f}; }
  static Spring snappy() { return {.response = 0.22f, .bounce = 0.15f}; }
  static Spring bouncy() { return {.response = 0.35f, .bounce = 0.35f}; }
  static Spring gentle() { return {.response = 0.5f, .bounce = 0.f}; }
  static Spring from_freq_decay(float freq, float decay) {
    freq = std::max(freq, 1e-4f);
    return {.response = 2.f * 3.14159265358979f / freq,
            .bounce = std::clamp(1.f - decay / freq, 0.f, 0.999f)};
  }
};

struct SpringState {
  float x0 = 0.f;
  float v0 = 0.f;
  float target = 0.f;
};

struct SpringSample {
  float x = 0.f;
  float v = 0.f;
};

static float spring_omega(const Spring &s) {
  return 2.f * 3.14159265358979f / std::max(s.response, 1e-4f);
}
static float spring_zeta(const Spring &s) {
  return 1.f - std::clamp(s.bounce, 0.f, 0.999f);
}

static SpringSample spring_solve(const Spring &s, const SpringState &st,
                                 float t) {
  t = std::max(t, 0.f);
  const float w = spring_omega(s);
  const float z = spring_zeta(s);
  const float d0 = st.x0 - st.target;
  const float v0 = st.v0;
  const float decay = std::exp(-z * w * t);
  if (z >= kCriticalZeta) {
    const float c = v0 + w * d0;
    const float d = decay * (d0 + c * t);
    const float v = decay * (v0 - w * c * t);
    return {st.target + d, v};
  }
  const float wd = w * std::sqrt(1.f - z * z);
  const float a = d0;
  const float b = (v0 + z * w * d0) / wd;
  const float cs = std::cos(wd * t), sn = std::sin(wd * t);
  const float d = decay * (a * cs + b * sn);
  const float v = decay * ((b * wd - z * w * a) * cs -
                           (a * wd + z * w * b) * sn);
  return {st.target + d, v};
}

static float spring_settle_time(const Spring &s, const SpringState &st) {
  const float w = spring_omega(s);
  const float z = spring_zeta(s);
  const float d0 = st.x0 - st.target;
  const float v0 = st.v0;
  float amp_d, amp_v, grow_d = 0.f, grow_v = 0.f;
  if (z >= kCriticalZeta) {
    const float c = v0 + w * d0;
    amp_d = std::fabs(d0);
    grow_d = std::fabs(c);
    amp_v = std::fabs(v0);
    grow_v = w * std::fabs(c);
  } else {
    const float wd = w * std::sqrt(1.f - z * z);
    const float b = (v0 + z * w * d0) / wd;
    amp_d = std::sqrt(d0 * d0 + b * b);
    const float vb = (w * w * d0 + z * w * v0) / wd;
    amp_v = std::sqrt(v0 * v0 + vb * vb);
  }
  const float scale = std::max(amp_d, amp_v / w);
  if (scale <= 0.f)
    return 0.f;
  const float eps_d = s.rest_delta > 0.f ? s.rest_delta : scale * kRestRatio;
  const float eps_v = s.rest_speed > 0.f ? s.rest_speed : eps_d * w;
  auto under = [&](float t) {
    const float decay = std::exp(-z * w * t);
    return decay * (amp_d + grow_d * t) <= eps_d &&
           decay * (amp_v + grow_v * t) <= eps_v;
  };
  float hi = s.response;
  for (int i = 0; i < 32 && !under(hi); ++i)
    hi *= 2.f;
  float lo = 0.f;
  for (int i = 0; i < 32; ++i) {
    const float mid = 0.5f * (lo + hi);
    (under(mid) ? hi : lo) = mid;
  }
  return hi;
}

} // namespace motion
} // namespace afterhours
