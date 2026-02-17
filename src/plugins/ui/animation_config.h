#pragma once

#include <cmath>
#include <vector>

#include "../../core/base_component.h"

namespace afterhours {
namespace ui {

// =============================================================================
// Animation Trigger - what starts the animation
// =============================================================================
enum class AnimTrigger {
  OnAppear, // First render
  OnClick,  // When clicked/pressed (active)
  OnHover,  // Mouse enters (hot)
  OnFocus,  // Keyboard focus
  Loop,     // Continuous animation
};

// =============================================================================
// Animation Property - what gets animated
// =============================================================================
enum class AnimProperty {
  Scale,
  TranslateX,
  TranslateY,
  Rotation,
  Opacity,
};

// =============================================================================
// Animation Curve - how interpolation works
// =============================================================================
enum class AnimCurve {
  Spring,    // Damped spring physics
  EaseOut,   // Fast start, slow end
  EaseIn,    // Slow start, fast end
  EaseInOut, // Slow both ends
  Linear,    // Constant speed
};

// =============================================================================
// Animation Definition - complete specification
// =============================================================================
struct AnimationDef {
  AnimTrigger trigger = AnimTrigger::OnClick;
  AnimProperty property = AnimProperty::Scale;
  float from_value = 1.0f;
  float to_value = 1.0f;
  AnimCurve curve = AnimCurve::Spring;

  // Timing (for non-spring curves)
  float duration = 0.15f;

  // Spring params (for Spring curve)
  float spring_frequency = 12.0f;
  float spring_decay = 8.0f;
};

// =============================================================================
// Anim Builder - fluent API for creating AnimationDef
// =============================================================================
class Anim {
  AnimationDef def;

public:
  Anim() = default;

  // Static factory methods for triggers
  static Anim on_appear() {
    Anim a;
    a.def.trigger = AnimTrigger::OnAppear;
    return a;
  }
  static Anim on_click() {
    Anim a;
    a.def.trigger = AnimTrigger::OnClick;
    return a;
  }
  static Anim on_hover() {
    Anim a;
    a.def.trigger = AnimTrigger::OnHover;
    return a;
  }
  static Anim on_focus() {
    Anim a;
    a.def.trigger = AnimTrigger::OnFocus;
    return a;
  }
  static Anim loop() {
    Anim a;
    a.def.trigger = AnimTrigger::Loop;
    return a;
  }

  // Property setters (from, to)
  Anim &scale(float from, float to) {
    def.property = AnimProperty::Scale;
    def.from_value = from;
    def.to_value = to;
    return *this;
  }
  Anim &translate_x(float from, float to) {
    def.property = AnimProperty::TranslateX;
    def.from_value = from;
    def.to_value = to;
    return *this;
  }
  Anim &translate_y(float from, float to) {
    def.property = AnimProperty::TranslateY;
    def.from_value = from;
    def.to_value = to;
    return *this;
  }
  Anim &rotate(float from, float to) {
    def.property = AnimProperty::Rotation;
    def.from_value = from;
    def.to_value = to;
    return *this;
  }
  Anim &opacity(float from, float to) {
    def.property = AnimProperty::Opacity;
    def.from_value = from;
    def.to_value = to;
    return *this;
  }

  // Shorthand: single value means "animate TO this value"
  // For scale: from 1.0 (default)
  // For translate: from 0.0 (default)
  // For rotate: from 0.0 (default, in degrees)
  // For opacity: from 1.0 (default)
  Anim &scale(float to) { return scale(1.0f, to); }
  Anim &translate_x(float to) { return translate_x(0.0f, to); }
  Anim &translate_y(float to) { return translate_y(0.0f, to); }
  Anim &rotate(float to) { return rotate(0.0f, to); }
  Anim &opacity(float to) { return opacity(1.0f, to); }

  // Curve setters
  Anim &spring(float freq = 12.0f, float decay = 8.0f) {
    def.curve = AnimCurve::Spring;
    def.spring_frequency = freq;
    def.spring_decay = decay;
    return *this;
  }
  Anim &ease_out(float dur = 0.15f) {
    def.curve = AnimCurve::EaseOut;
    def.duration = dur;
    return *this;
  }
  Anim &ease_in(float dur = 0.15f) {
    def.curve = AnimCurve::EaseIn;
    def.duration = dur;
    return *this;
  }
  Anim &ease_in_out(float dur = 0.15f) {
    def.curve = AnimCurve::EaseInOut;
    def.duration = dur;
    return *this;
  }
  Anim &linear(float dur = 0.15f) {
    def.curve = AnimCurve::Linear;
    def.duration = dur;
    return *this;
  }

  // Duration setter (can be chained after curve)
  Anim &duration(float dur) {
    def.duration = dur;
    return *this;
  }

  // Get the built definition
  const AnimationDef &build() const { return def; }
};

// =============================================================================
// AnimTrack - minimal per-property state
// =============================================================================
struct AnimTrack {
  float current = 1.0f;   // Current animated value
  float from = 1.0f;      // Starting value
  float target = 1.0f;    // Target value
  float velocity = 0.0f;  // For spring physics
  float elapsed = 0.0f;   // For timed curves
  bool is_active = false; // Is currently animating
  bool triggered = false; // Was trigger active last frame (for edge detection)
};

// =============================================================================
// HasAnimationState - per-entity component
// =============================================================================
struct HasAnimationState : BaseComponent {
  AnimTrack scale;
  AnimTrack translate_x;
  AnimTrack translate_y;
  AnimTrack rotation;
  AnimTrack opacity;

  // Track if entity has appeared (for OnAppear trigger)
  bool has_appeared = false;

  HasAnimationState() {
    // Initialize to default values
    scale.current = 1.0f;
    scale.target = 1.0f;
    translate_x.current = 0.0f;
    translate_x.target = 0.0f;
    translate_y.current = 0.0f;
    translate_y.target = 0.0f;
    rotation.current = 0.0f;
    rotation.target = 0.0f;
    opacity.current = 1.0f;
    opacity.target = 1.0f;
  }

  AnimTrack &get(AnimProperty prop) {
    switch (prop) {
    case AnimProperty::Scale:
      return scale;
    case AnimProperty::TranslateX:
      return translate_x;
    case AnimProperty::TranslateY:
      return translate_y;
    case AnimProperty::Rotation:
      return rotation;
    case AnimProperty::Opacity:
      return opacity;
    }
    return scale; // fallback
  }
};

// =============================================================================
// anim namespace - pure animation functions
// =============================================================================
namespace anim {

// Start an animation from -> to
inline void start(AnimTrack &t, float from, float to) {
  t.from = from;
  t.current = from;
  t.target = to;
  t.velocity = 0.0f;
  t.elapsed = 0.0f;
  t.is_active = true;
}

// Start animation from current value to target
inline void start_to(AnimTrack &t, float to) {
  t.from = t.current;
  t.target = to;
  t.velocity = 0.0f;
  t.elapsed = 0.0f;
  t.is_active = true;
}

// Spring physics update - returns true when settled
inline bool spring(AnimTrack &t, float freq, float decay, float dt) {
  if (!t.is_active)
    return true;

  float delta = t.target - t.current;
  float spring_force = delta * freq * freq;
  float damping_force = -t.velocity * 2.0f * decay;
  t.velocity += (spring_force + damping_force) * dt;
  t.current += t.velocity * dt;

  // Check if settled
  if (std::abs(delta) < 0.001f && std::abs(t.velocity) < 0.001f) {
    t.current = t.target;
    t.velocity = 0.0f;
    t.is_active = false;
    return true;
  }
  return false;
}

// Apply easing function
inline float apply_easing(float t, AnimCurve curve) {
  switch (curve) {
  case AnimCurve::Linear:
    return t;
  case AnimCurve::EaseIn:
    return t * t;
  case AnimCurve::EaseOut:
    return 1.0f - (1.0f - t) * (1.0f - t);
  case AnimCurve::EaseInOut:
    return t < 0.5f ? 2.0f * t * t
                    : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
  case AnimCurve::Spring:
    return t; // Spring doesn't use this
  }
  return t;
}

// Timed easing update - returns true when complete
inline bool ease(AnimTrack &t, AnimCurve curve, float duration, float dt) {
  if (!t.is_active)
    return true;

  t.elapsed += dt;
  float progress = std::min(t.elapsed / duration, 1.0f);
  float eased = apply_easing(progress, curve);
  t.current = t.from + (t.target - t.from) * eased;

  if (progress >= 1.0f) {
    t.current = t.target;
    t.is_active = false;
    return true;
  }
  return false;
}

// Update a track based on AnimationDef
inline void update(AnimTrack &t, const AnimationDef &def, float dt) {
  switch (def.curve) {
  case AnimCurve::Spring:
    spring(t, def.spring_frequency, def.spring_decay, dt);
    return;
  case AnimCurve::EaseOut:
  case AnimCurve::EaseIn:
  case AnimCurve::EaseInOut:
  case AnimCurve::Linear:
    ease(t, def.curve, def.duration, dt);
    return;
  }
}

} // namespace anim

} // namespace ui
} // namespace afterhours
