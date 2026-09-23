#pragma once

#include <array>
#include <cmath>

#include "animation.h"
#include "ui_motion.h"

namespace afterhours {
namespace presets {

inline ui_motion::MotionRule fade_up(float distance = 12.f, float seconds = 0.5f, float delay = 0.f) {
  return {ui_motion::MotionTrigger::Appear,
          {.translate_y = {distance, 0.f}, .opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad},
          delay};
}

inline ui_motion::MotionRule fade_in(float seconds = 0.25f, float delay = 0.f) {
  return {ui_motion::MotionTrigger::Appear, {.opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad}, delay};
}

inline ui_motion::MotionRule pop_in(float from_scale = 0.3f, motion::Spring spring = motion::Spring::bouncy()) {
  return {ui_motion::MotionTrigger::Appear, {.scale = {from_scale, 1.f}, .opacity = {0.f, 1.f}}, spring};
}

inline ui_motion::MotionRule hover_lift(float pixels = 4.f, float scale = 1.05f,
                                 motion::Spring spring = motion::Spring::snappy()) {
  return {ui_motion::MotionTrigger::Hover, {.scale = scale, .translate_y = -pixels}, spring};
}

inline ui_motion::MotionRule press_squash(float scale = 0.92f, motion::Spring spring = motion::Spring::snappy()) {
  return {ui_motion::MotionTrigger::Press, {.scale = scale}, spring};
}

inline ui_motion::MotionRule slide_in(float from_x, float seconds = 0.25f, float delay = 0.f) {
  return {ui_motion::MotionTrigger::Appear, {.translate_x = {from_x, 0.f}, .opacity = {0.f, 1.f}},
          motion::Timeline{.keys = {{0.f, 0.f}, {seconds, 1.f}}, .curve = motion::curves::ease_out_quad}, delay};
}

inline motion::Timeline shake(float seconds = 0.28f) {
  return {.keys = {{0.f, 0.f}, {seconds * 0.2857f, 1.f}, {seconds * 0.5714f, -1.f}, {seconds * 0.7857f, 0.667f}, {seconds, 0.f}},
          .curve = motion::curves::ease_out_quad};
}

inline motion::Timeline spin(float seconds = 0.9f) {
  return {.keys = {{0.f, 0.f}, {seconds, 1.f}}, .repeat = motion::Timeline::Repeat::Loop};
}

inline motion::Timeline pulse(float seconds = 0.5f) {
  return {.keys = {{0.f, 0.f}, {seconds, 1.f}}, .repeat = motion::Timeline::Repeat::PingPong,
          .curve = motion::curves::ease_in_out_quad};
}

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

} // namespace presets
} // namespace afterhours
