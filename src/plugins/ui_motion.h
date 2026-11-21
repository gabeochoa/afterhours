#pragma once

#include <array>
#include <optional>
#include <vector>

#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../vendor/magic_enum/magic_enum.hpp"
#endif

#include "../core/base_component.h"
#include "animation.h"
#include "ui/context.h"
#include "ui/component_config.h"
#include "ui/entity_management.h"
#include "ui/components.h"

namespace afterhours {
namespace ui_motion {

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

struct MotionColor {
  bool set = false;
  bool has_from = false;
  ColorType from{};
  ColorType to{};
  MotionColor() = default;
  MotionColor(ColorType value) : set(true), to(value) {}
  MotionColor(ColorType start, ColorType end)
      : set(true), has_from(true), from(start), to(end) {}
};

struct MotionProps {
  MotionProp scale;
  MotionProp translate_x;
  MotionProp translate_y;
  MotionProp rotation;
  MotionProp opacity;
  MotionProp corner_radius;
  MotionProp blur;
  MotionColor background;
};

enum struct MotionProperty : size_t {
  Scale,
  TranslateX,
  TranslateY,
  Rotation,
  Opacity,
  CornerRadius,
  Blur
};
constexpr size_t kMotionProperties = magic_enum::enum_count<MotionProperty>();

enum struct MotionTrigger { Appear, State, Focus, Hover, Press, Change, Exit };

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
  case MotionProperty::CornerRadius:
    return p.corner_radius;
  case MotionProperty::Blur:
    return p.blur;
  case MotionProperty::Opacity:
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
  bool has_exit = false;
  bool exiting = false;
  bool exit_done = false;
  MotionProps exit_props;
  motion::Mode exit_mode = motion::Spring::smooth();
  float exit_delay = 0.f;
  bool had_state = false;
  bool last_state = false;
  std::vector<std::optional<size_t>> stamps;
  std::array<float, kMotionProperties> rest{};
  std::array<motion::Mode, kMotionProperties> release_mode{};
  motion::Mode background_release = motion::Spring::smooth();
  std::optional<ColorType> background_rest;
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

struct MotionColorTarget {
  bool mentioned = false;
  bool set = false;
  ColorType value{};
  std::optional<ColorType> rest;
  std::optional<ColorType> reset_to;
  motion::Mode mode = motion::Spring::smooth();
  float delay = 0.f;
};

struct MotionTargets {
  std::array<MotionTarget, kMotionProperties> props{};
  MotionColorTarget background;
  MotionTarget &operator[](size_t i) { return props[i]; }
  const MotionTarget &operator[](size_t i) const { return props[i]; }
};

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
    const MotionColor &c = r.props.background;
    if (c.set) {
      MotionColorTarget &t = out.background;
      t.set = true;
      t.value = c.to;
      t.mode = r.mode;
      t.delay = with_delay ? r.delay : 0.f;
      if (with_reset && c.has_from)
        t.reset_to = c.from;
      st.background_release = r.mode;
    }
  };

  for (const MotionRule &r : rules) {
    if (r.trigger == MotionTrigger::Exit)
      continue;
    for (size_t i = 0; i < kMotionProperties; ++i)
      mentioned[i] = mentioned[i] || motion_prop(r.props, i).set;
    out.background.mentioned = out.background.mentioned || r.props.background.set;
  }

  for (const MotionRule &r : rules) {
    if (r.trigger != MotionTrigger::Appear)
      continue;
    for (size_t i = 0; i < kMotionProperties; ++i)
      if (motion_prop(r.props, i).set)
        st.rest[i] = motion_prop(r.props, i).to;
    if (r.props.background.set)
      st.background_rest = r.props.background.to;
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
    if (!r.state && r.props.background.set) {
      if (r.props.background.has_from)
        out.background.value = r.props.background.from;
      else
        out.background.set = false;
    }
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
  if (out.background.mentioned && !out.background.set)
    out.background.mode = st.background_release;
  out.background.rest = st.background_rest;
  return out;
}

struct MotionRests {
  float corner_radius = 0.f;
  ColorType background{};
};

struct MotionValues {
  float scale = 1.f;
  float translate_x = 0.f;
  float translate_y = 0.f;
  float rotation = 0.f;
  float opacity = 1.f;
  std::optional<float> corner_radius;
  std::optional<float> blur;
  std::optional<ColorType> background;
};

inline bool colors_match(ColorType a, ColorType b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

template <typename Ctx>
inline MotionValues apply_motion(Ctx &ctx, Entity &entity,
                                 const std::vector<MotionRule> &rules,
                                 const MotionRests &rests = {}) {
  MotionValues out;
  if (rules.empty())
    return out;
  auto &st = entity.template addComponentIfMissing<HasMotionState>();
  auto &tracks = entity.template addComponentIfMissing<motion::HasTracks>();
  for (const MotionRule &r : rules) {
    if (r.trigger != MotionTrigger::Exit)
      continue;
    st.has_exit = true;
    st.exit_props = r.props;
    st.exit_mode = r.mode;
    st.exit_delay = r.delay;
  }
  if (st.exiting) { // rebuilt mid-exit: the caller wants it back
    st.exiting = false;
    st.exit_done = false;
    entity.template removeComponentIfExists<ui::imm::HasExitPresence>();
  }
  st.rest[static_cast<size_t>(MotionProperty::CornerRadius)] = rests.corner_radius;
  const MotionInputs in{ctx.was_hot(entity.id), ctx.was_active(entity.id),
                        ctx.has_focus(entity.id)};
  const MotionTargets targets = resolve_motion(rules, in, st);

  if (targets.background.mentioned) {
    const MotionColorTarget &t = targets.background;
    auto &tr = tracks.track<ColorType>(0);
    const ColorType goal = t.set ? t.value : t.rest.value_or(rests.background);
    if (t.reset_to.has_value())
      tr.from(*t.reset_to);
    else if (!tr.started())
      tr.from(rests.background);
    if (!colors_match(tr.target(), goal)) {
      tr.to(goal, t.mode);
      if (t.delay > 0.f)
        tr.delay(t.delay);
    }
    out.background = tr.value();
  }

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
  if (auto it = tracks.floats.find(static_cast<size_t>(MotionProperty::CornerRadius));
      it != tracks.floats.end())
    out.corner_radius = it->second.value();
  if (auto it = tracks.floats.find(static_cast<size_t>(MotionProperty::Blur));
      it != tracks.floats.end())
    out.blur = it->second.value();
  return out;
}

// Start (first call) or poll the exit: true while the exit track is running.
// Instant motion finishes on the spot, so callers retire immediately.
inline bool exit_retention(Entity &entity) {
  if (!entity.has<HasMotionState>())
    return false;
  auto &st = entity.get<HasMotionState>();
  if (!st.has_exit || st.exit_done)
    return false;
  if (motion::is_instant()) {
    st.exit_done = true;
    // Marker so the sweep retires it now instead of after the grace window.
    entity.template addComponentIfMissing<ui::imm::HasExitPresence>();
    return false;
  }
  auto &tracks = entity.template addComponentIfMissing<motion::HasTracks>();
  if (!st.exiting) {
    st.exiting = true;
    for (size_t i = 0; i < kMotionProperties; ++i) {
      const MotionProp &prop = motion_prop(st.exit_props, i);
      if (!prop.set)
        continue;
      auto &tr = tracks.track<float>(i);
      if (!tr.started())
        tr.from(st.rest[i]);
      tr.to(prop.to, st.exit_mode);
      if (st.exit_delay > 0.f)
        tr.delay(st.exit_delay);
    }
    if (st.exit_props.background.set) {
      auto &tr = tracks.track<ColorType>(0);
      if (!tr.started() && st.background_rest.has_value())
        tr.from(*st.background_rest);
      tr.to(st.exit_props.background.to, st.exit_mode);
      if (st.exit_delay > 0.f)
        tr.delay(st.exit_delay);
    }
    return true;
  }
  bool active = false;
  for (size_t i = 0; i < kMotionProperties; ++i)
    if (motion_prop(st.exit_props, i).set) {
      auto it = tracks.floats.find(i);
      active = active || (it != tracks.floats.end() && it->second.active());
    }
  if (st.exit_props.background.set) {
    auto it = tracks.colors.find(0);
    active = active || (it != tracks.colors.end() && it->second.active());
  }
  if (!active) {
    st.exiting = false;
    st.exit_done = true;
  }
  return active;
}

// Write the current exit-track values onto the widget for a frame in which
// nobody rebuilt it (called by PresentExitingWidgets / tests).
inline void apply_exit_frame(Entity &entity) {
  if (!entity.has<HasMotionState>() || !entity.has<motion::HasTracks>())
    return;
  auto &tracks = entity.get<motion::HasTracks>();
  auto &st = entity.get<HasMotionState>();
  auto &mods = entity.template addComponentIfMissing<ui::HasUIModifiers>();
  mods.scale = 1.f;
  mods.translate_x = 0.f;
  mods.translate_y = 0.f;
  mods.rotation = 0.f;
  auto read = [&](MotionProperty prop, float rest) {
    auto it = tracks.floats.find(static_cast<size_t>(prop));
    return it == tracks.floats.end() ? rest : it->second.value();
  };
  if (st.exit_props.scale.set)
    mods.scale = read(MotionProperty::Scale, 1.f);
  if (st.exit_props.translate_x.set)
    mods.translate_x = read(MotionProperty::TranslateX, 0.f);
  if (st.exit_props.translate_y.set)
    mods.translate_y = read(MotionProperty::TranslateY, 0.f);
  if (st.exit_props.rotation.set)
    mods.rotation = read(MotionProperty::Rotation, 0.f);
  if (st.exit_props.opacity.set)
    entity.template addComponentIfMissing<ui::HasOpacity>().value =
        read(MotionProperty::Opacity, 1.f);
  if (st.exit_props.background.set) {
    auto it = tracks.colors.find(0);
    if (it != tracks.colors.end() && entity.has<HasColor>())
      entity.get<HasColor>().set(it->second.value());
  }
}

// Re-presents exiting widgets: runs after user builds, before autolayout, so
// a departing widget keeps its last layout while its exit track runs.
struct PresentExitingWidgets : System<ui::imm::HasExitPresence> {
  void for_each_with(Entity &entity, ui::imm::HasExitPresence &xp,
                     float) override {
    if (!xp.active || xp.parent_id < 0)
      return;
    auto parent = ui::UICollectionHolder::getEntityForID(xp.parent_id);
    if (parent.valid() && parent->template has<ui::UIComponent>())
      parent->template get<ui::UIComponent>().add_child(entity.id);
    apply_exit_frame(entity);
  }
};

struct MotionExt {
  MotionRule rule;
};

inline MotionExt on_appear(MotionProps props,
                           motion::Mode mode = motion::Spring::smooth(),
                           float delay = 0.f) {
  return {{MotionTrigger::Appear, props, std::move(mode), delay}};
}
inline MotionExt on_hover(MotionProps props,
                          motion::Mode mode = motion::Spring::snappy()) {
  return {{MotionTrigger::Hover, props, std::move(mode)}};
}
inline MotionExt on_press(MotionProps props,
                          motion::Mode mode = motion::Spring::snappy()) {
  return {{MotionTrigger::Press, props, std::move(mode)}};
}
inline MotionExt on_focus(MotionProps props,
                          motion::Mode mode = motion::Spring::snappy()) {
  return {{MotionTrigger::Focus, props, std::move(mode)}};
}
inline MotionExt on_state(bool state, MotionProps props,
                          motion::Mode mode = motion::Spring::smooth()) {
  MotionRule r{MotionTrigger::State, props, std::move(mode)};
  r.state = state;
  return {r};
}
inline MotionExt on_exit(MotionProps props,
                         motion::Mode mode = motion::Spring::smooth(),
                         float delay = 0.f) {
  return {{MotionTrigger::Exit, props, std::move(mode), delay}};
}
inline MotionExt on_change(size_t stamp, MotionProps props,
                           motion::Mode mode = motion::Spring::bouncy()) {
  MotionRule r{MotionTrigger::Change, props, std::move(mode)};
  r.stamp = stamp;
  return {r};
}

inline std::vector<MotionRule> motion_rules(const ui::imm::ComponentConfig &config) {
  std::vector<MotionRule> rules;
  for (const MotionExt *ext : ui::imm::extensions_all<MotionExt>(config))
    rules.push_back(ext->rule);
  return rules;
}

template <typename Ctx>
inline void apply_motion_hook(Ctx &ctx, Entity &entity,
                              const ui::imm::ComponentConfig &config) {
  const std::vector<MotionRule> rules = motion_rules(config);
  if (rules.empty())
    return;
  MotionRests rests;
  rests.corner_radius = config.corner_radius.value_or(0.f);
  if (entity.has<HasColor>())
    rests.background = entity.get<HasColor>().color();
  const MotionValues mv = apply_motion(ctx, entity, rules, rests);
  auto &mods = entity.addComponentIfMissing<ui::HasUIModifiers>();
  mods.scale *= mv.scale;
  mods.translate_x += mv.translate_x;
  mods.translate_y += mv.translate_y;
  mods.rotation += mv.rotation;
  if (mv.opacity != 1.f)
    entity.addComponentIfMissing<ui::HasOpacity>().value *= mv.opacity;
  if (mv.corner_radius.has_value()) {
    auto &rc = entity.addComponentIfMissing<ui::HasRoundedCorners>();
    if (!rc.rounded_corners.any())
      rc.set(std::bitset<4>().set());
    rc.set_radius_px(*mv.corner_radius);
  }
  if (mv.background.has_value()) {
    auto &hc = entity.addComponentIfMissing<HasColor>(*mv.background);
    hc.set(*mv.background);
    hc.skip_hover_override = true;
  }
  if (mv.blur.has_value())
    entity.addComponentIfMissing<ui::HasBlur>().radius = *mv.blur;
  else if (config.blur == 0.f)
    entity.removeComponentIfExists<ui::HasBlur>();
}

template <typename Ctx> inline void register_bridge() {
  static const bool once = [] {
    ui::imm::register_init_hook([](void *raw, Entity &entity,
                                   const ui::imm::ComponentConfig &config) {
      apply_motion_hook(*static_cast<Ctx *>(raw), entity, config);
    });
    ui::imm::register_ui_extension_system(
        [] { return std::make_unique<motion::AdvanceTracks>(); });
    ui::imm::ui_exit_retention_hook = [](Entity &e) { return exit_retention(e); };
    ui::imm::register_ui_post_update_system(
        [] { return std::make_unique<PresentExitingWidgets>(); });
    return true;
  }();
  (void)once;
}

} // namespace ui_motion
} // namespace afterhours
