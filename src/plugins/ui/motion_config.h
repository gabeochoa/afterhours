#pragma once

#include <array>
#include <optional>
#include <vector>

#include "../../core/base_component.h"
#include "../animation.h"

namespace afterhours {
namespace ui {

struct MotionProp {
  bool set = false;
  bool has_from = false;
  float from = 0.f;
  float to = 0.f;
  MotionProp() = default;
  MotionProp(float value) : set(true), to(value) {}
  MotionProp(float start, float end)
      : set(true), has_from(true), from(start), to(end) {}
};

struct MotionProps {
  MotionProp scale;
  MotionProp translate_x;
  MotionProp translate_y;
  MotionProp rotation;
  MotionProp opacity;
};

enum struct MotionProperty : size_t {
  Scale,
  TranslateX,
  TranslateY,
  Rotation,
  Opacity,
  Count
};
constexpr size_t kMotionProperties = static_cast<size_t>(MotionProperty::Count);

enum struct MotionTrigger { Appear, State, Focus, Hover, Press, Change };

struct MotionRule {
  MotionTrigger trigger = MotionTrigger::Hover;
  MotionProps props;
  motion::Mode mode = motion::Spring::smooth();
  float delay = 0.f;
  bool state = false;
  size_t stamp = 0;
};

inline const MotionProp &motion_prop(const MotionProps &p, size_t i) {
  switch (static_cast<MotionProperty>(i)) {
  case MotionProperty::Scale:
    return p.scale;
  case MotionProperty::TranslateX:
    return p.translate_x;
  case MotionProperty::TranslateY:
    return p.translate_y;
  case MotionProperty::Rotation:
    return p.rotation;
  case MotionProperty::Opacity:
  case MotionProperty::Count:
    break;
  }
  return p.opacity;
}

inline float motion_default_rest(size_t i) {
  const auto p = static_cast<MotionProperty>(i);
  return (p == MotionProperty::Scale || p == MotionProperty::Opacity) ? 1.f
                                                                      : 0.f;
}

struct HasMotionState : BaseComponent {
  bool appeared = false;
  bool had_state = false;
  bool last_state = false;
  std::vector<std::optional<size_t>> stamps;
  std::array<float, kMotionProperties> rest{};
  std::array<motion::Mode, kMotionProperties> release_mode{};
  HasMotionState() {
    for (size_t i = 0; i < kMotionProperties; ++i) {
      rest[i] = motion_default_rest(i);
      release_mode[i] = motion::Spring::smooth();
    }
  }
};

struct MotionInputs {
  bool hot = false;
  bool active = false;
  bool focus = false;
};

struct MotionTarget {
  bool set = false;
  float value = 0.f;
  std::optional<float> reset_to;
  motion::Mode mode = motion::Spring::smooth();
  float delay = 0.f;
};
using MotionTargets = std::array<MotionTarget, kMotionProperties>;

inline MotionTargets resolve_motion(const std::vector<MotionRule> &rules,
                                    const MotionInputs &in,
                                    HasMotionState &st) {
  MotionTargets out{};
  std::array<bool, kMotionProperties> mentioned{};
  const bool first_frame = !st.appeared;
  st.appeared = true;

  auto assign = [&](const MotionRule &r, auto value_of, bool with_delay,
                    bool with_reset) {
    for (size_t i = 0; i < kMotionProperties; ++i) {
      const MotionProp &p = motion_prop(r.props, i);
      if (!p.set)
        continue;
      MotionTarget &t = out[i];
      t.set = true;
      t.value = value_of(p, i);
      t.mode = r.mode;
      t.delay = with_delay ? r.delay : 0.f;
      if (with_reset && p.has_from)
        t.reset_to = p.from;
      st.release_mode[i] = r.mode;
    }
  };

  for (const MotionRule &r : rules)
    for (size_t i = 0; i < kMotionProperties; ++i)
      mentioned[i] = mentioned[i] || motion_prop(r.props, i).set;

  for (const MotionRule &r : rules) {
    if (r.trigger != MotionTrigger::Appear)
      continue;
    for (size_t i = 0; i < kMotionProperties; ++i)
      if (motion_prop(r.props, i).set)
        st.rest[i] = motion_prop(r.props, i).to;
    if (first_frame)
      assign(r, [](const MotionProp &p, size_t) { return p.to; }, true, true);
  }

  for (const MotionRule &r : rules) {
    if (r.trigger != MotionTrigger::State)
      continue;
    assign(
        r,
        [&](const MotionProp &p, size_t i) {
          if (r.state)
            return p.to;
          return p.has_from ? p.from : st.rest[i];
        },
        false, false);
    st.had_state = true;
    st.last_state = r.state;
  }

  for (MotionTrigger trig :
       {MotionTrigger::Focus, MotionTrigger::Hover, MotionTrigger::Press}) {
    const bool on = trig == MotionTrigger::Focus   ? in.focus
                    : trig == MotionTrigger::Hover ? in.hot
                                                   : in.active;
    if (!on)
      continue;
    for (const MotionRule &r : rules)
      if (r.trigger == trig)
        assign(r, [](const MotionProp &p, size_t) { return p.to; }, false,
               false);
  }

  if (st.stamps.size() < rules.size())
    st.stamps.resize(rules.size());
  for (size_t idx = 0; idx < rules.size(); ++idx) {
    const MotionRule &r = rules[idx];
    if (r.trigger != MotionTrigger::Change)
      continue;
    std::optional<size_t> &seen = st.stamps[idx];
    const bool fired = seen.has_value() && *seen != r.stamp;
    seen = r.stamp;
    if (fired)
      assign(r, [](const MotionProp &p, size_t) { return p.to; }, false, true);
  }

  for (size_t i = 0; i < kMotionProperties; ++i) {
    if (out[i].set || !mentioned[i])
      continue;
    out[i].set = true;
    out[i].value = st.rest[i];
    out[i].mode = st.release_mode[i];
  }
  return out;
}

struct MotionValues {
  float scale = 1.f;
  float translate_x = 0.f;
  float translate_y = 0.f;
  float rotation = 0.f;
  float opacity = 1.f;
};

template <typename Ctx>
inline MotionValues apply_motion(Ctx &ctx, Entity &entity,
                                 const std::vector<MotionRule> &rules) {
  MotionValues out;
  if (rules.empty())
    return out;
  auto &st = entity.template addComponentIfMissing<HasMotionState>();
  auto &tracks = entity.template addComponentIfMissing<motion::HasTracks>();
  const MotionInputs in{ctx.was_hot(entity.id), ctx.was_active(entity.id),
                        ctx.has_focus(entity.id)};
  const MotionTargets targets = resolve_motion(rules, in, st);

  for (size_t i = 0; i < kMotionProperties; ++i) {
    const MotionTarget &t = targets[i];
    if (!t.set)
      continue;
    auto &tr = tracks.track<float>(i);
    if (t.reset_to.has_value())
      tr.from(*t.reset_to);
    else if (!tr.started())
      tr.from(st.rest[i]);
    if (tr.target() != t.value) {
      tr.to(t.value, t.mode);
      if (t.delay > 0.f)
        tr.delay(t.delay);
    }
  }

  auto read = [&](MotionProperty p, float &into, bool multiply) {
    auto it = tracks.floats.find(static_cast<size_t>(p));
    if (it == tracks.floats.end())
      return;
    if (multiply)
      into *= it->second.value();
    else
      into += it->second.value();
  };
  read(MotionProperty::Scale, out.scale, true);
  read(MotionProperty::TranslateX, out.translate_x, false);
  read(MotionProperty::TranslateY, out.translate_y, false);
  read(MotionProperty::Rotation, out.rotation, false);
  read(MotionProperty::Opacity, out.opacity, true);
  return out;
}

} // namespace ui
} // namespace afterhours
