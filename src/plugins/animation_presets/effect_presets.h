#pragma once

#include <array>
#include <cmath>

#include "../../rect.h"
#include "../animation.h"

namespace afterhours {
namespace presets {

enum struct EffectId {
  Blur,
  Unblur,
  Dissolve,
  Wipe,
  Curtain,
  Sweep,
  Iris,
  Spotlight,
  Blinds,
  Unroll,
  Shear,
  Stretch,
  Flip,
  Tumble,
  Swing,
  Recede,
  Emerge
};

enum struct EffectShader {
  None = -1,
  Dissolve = 0,
  Wipe = 1,
  Curtain = 2,
  Sweep = 3,
  Iris = 4,
  Spotlight = 5,
  Blinds = 6,
  Unroll = 7
};

struct EffectPreset {
  EffectId id;
  EffectShader shader = EffectShader::None;
};

inline constexpr std::array<EffectPreset, 17> effect_presets{{
    {EffectId::Blur, EffectShader::None},
    {EffectId::Unblur, EffectShader::None},
    {EffectId::Dissolve, EffectShader::Dissolve},
    {EffectId::Wipe, EffectShader::Wipe},
    {EffectId::Curtain, EffectShader::Curtain},
    {EffectId::Sweep, EffectShader::Sweep},
    {EffectId::Iris, EffectShader::Iris},
    {EffectId::Spotlight, EffectShader::Spotlight},
    {EffectId::Blinds, EffectShader::Blinds},
    {EffectId::Unroll, EffectShader::Unroll},
    {EffectId::Shear, EffectShader::None},
    {EffectId::Stretch, EffectShader::None},
    {EffectId::Flip, EffectShader::None},
    {EffectId::Tumble, EffectShader::None},
    {EffectId::Swing, EffectShader::None},
    {EffectId::Recede, EffectShader::None},
    {EffectId::Emerge, EffectShader::None},
}};

inline const EffectPreset *effect_preset(EffectId id) {
  for (const EffectPreset &e : effect_presets)
    if (e.id == id)
      return &e;
  return nullptr;
}

inline motion::Timeline effect_timeline(float seconds = 0.9f) {
  return {.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad};
}

inline float effect_blur_radius(const EffectPreset &e, float p) {
  if (e.id != EffectId::Blur && e.id != EffectId::Unblur)
    return 0.f;
  return (e.id == EffectId::Unblur ? 1.f - p : p) * 6.f;
}

struct EffectQuad {
  Vector2Type corners[4];
  float opacity = 1.f;
};

inline EffectQuad effect_quad(const EffectPreset &e, RectangleType r, float p) {
  EffectQuad q;
  q.corners[0] = {r.x, r.y};
  q.corners[1] = {r.x + r.width, r.y};
  q.corners[2] = {r.x + r.width, r.y + r.height};
  q.corners[3] = {r.x, r.y + r.height};
  const float cx = r.x + r.width / 2.f, cy = r.y + r.height / 2.f;
  auto rotate = [&](Vector2Type pt, Vector2Type pivot, float rad) {
    const float c = std::cos(rad), s = std::sin(rad);
    const float dx = pt.x - pivot.x, dy = pt.y - pivot.y;
    return Vector2Type{pivot.x + dx * c - dy * s, pivot.y + dx * s + dy * c};
  };
  auto scale_about = [&](Vector2Type pivot, float sx, float sy) {
    for (Vector2Type &pt : q.corners) {
      pt.x = pivot.x + (pt.x - pivot.x) * sx;
      pt.y = pivot.y + (pt.y - pivot.y) * sy;
    }
  };
  switch (e.id) {
  case EffectId::Shear: {
    const float off = (p - 0.5f) * 0.5f * r.width;
    q.corners[0].x += off;
    q.corners[1].x += off;
    q.corners[2].x -= off;
    q.corners[3].x -= off;
    break;
  }
  case EffectId::Stretch:
    scale_about({cx, cy}, 1.f + p * 0.5f, 1.f - p * 0.25f);
    break;
  case EffectId::Flip:
    scale_about({cx, cy}, std::cos(p * 3.14159265f), 1.f);
    break;
  case EffectId::Tumble:
    for (Vector2Type &pt : q.corners)
      pt = rotate(pt, {cx, cy}, p * 1.2f);
    scale_about({cx, cy}, 1.f, std::max(0.2f, std::cos(p * 3.14159265f)));
    break;
  case EffectId::Swing:
    for (Vector2Type &pt : q.corners)
      pt = rotate(pt, {cx, r.y}, (1.f - p) * -0.45f);
    break;
  case EffectId::Recede:
    scale_about({cx, cy}, 1.f - p * 0.3f, 1.f - p * 0.3f);
    q.opacity = 1.f - p * 0.75f;
    break;
  case EffectId::Emerge:
    scale_about({cx, cy}, 0.65f + p * 0.35f, 0.65f + p * 0.35f);
    q.opacity = p;
    break;
  default:
    break;
  }
  return q;
}

struct Shimmer {
  bool enabled = true;
  float period = 1.6f;
  float band = 0.35f;
  ColorType base{0, 0, 0, 255};
  ColorType highlight{255, 255, 255, 255};
  int strips = 16;
};

struct ShimmerStrip {
  RectangleType rect;
  ColorType color;
};

inline std::array<ShimmerStrip, 32> shimmer_strips(const Shimmer &s,
                                                   RectangleType r, float t,
                                                   int &count) {
  std::array<ShimmerStrip, 32> out{};
  count = 0;
  if (r.width <= 0.f || r.height <= 0.f)
    return out;
  if (!s.enabled || s.period <= 0.f) {
    out[0] = {r, s.base};
    count = 1;
    return out;
  }
  const int n = std::clamp(s.strips, 1, 32);
  const float phase = std::fmod(t, s.period) / s.period;
  const float band = std::clamp(s.band, 0.01f, 2.f);
  const float center = (-band / 2.f + phase * (1.f + band)) * r.width;
  const float half = band / 2.f * r.width;
  const float w = r.width / n;
  for (int i = 0; i < n; ++i) {
    const float x = r.x + i * w;
    const float xc = x + w / 2.f - r.x;
    const float k = std::clamp(1.f - std::abs(xc - center) / half, 0.f, 1.f);
    ColorType c;
    c.r = static_cast<unsigned char>(s.base.r + (s.highlight.r - s.base.r) * k);
    c.g = static_cast<unsigned char>(s.base.g + (s.highlight.g - s.base.g) * k);
    c.b = static_cast<unsigned char>(s.base.b + (s.highlight.b - s.base.b) * k);
    c.a = static_cast<unsigned char>(s.base.a + (s.highlight.a - s.base.a) * k);
    out[i] = {{x, r.y, w, r.height}, c};
  }
  count = n;
  return out;
}

} // namespace presets
} // namespace afterhours
