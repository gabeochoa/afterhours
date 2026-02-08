#pragma once

#include <functional>

#include "../../ecs.h"
#include "../color.h"
#include "../texture_manager.h"
#include "../window_manager.h"
#include "component_config.h"
#include "components.h"
#include "context.h"
#include "entity_management.h"

namespace afterhours {

namespace ui {

// Definition of UIContext::styling_defaults() (declared in context.h)
template <typename InputAction>
imm::UIStylingDefaults &UIContext<InputAction>::styling_defaults() {
  return imm::UIStylingDefaults::get();
}

namespace imm {

// Implementation of UIStylingDefaults methods that depend on ComponentConfig
inline UIStylingDefaults &
UIStylingDefaults::set_component_config(ComponentType component_type,
                                        const ComponentConfig &config) {
  component_configs[component_type] = config;
  return *this;
}

inline std::optional<ComponentConfig>
UIStylingDefaults::get_component_config(ComponentType component_type) const {
  auto it = component_configs.find(component_type);
  if (it != component_configs.end()) {
    return it->second;
  }
  return std::nullopt;
}

inline bool
UIStylingDefaults::has_component_defaults(ComponentType component_type) const {
  return component_configs.find(component_type) != component_configs.end();
}

inline ComponentConfig
UIStylingDefaults::merge_with_defaults(ComponentType component_type,
                                       const ComponentConfig &config) const {
  auto defaults = get_component_config(component_type);
  ComponentConfig result = config;

  // Apply default font name if the user didn't set one explicitly
  if (result.font_name == UIComponent::UNSET_FONT &&
      default_font_name != UIComponent::UNSET_FONT) {
    result.font_name = default_font_name;
  }

  // Apply default font size only if the user didn't explicitly set one
  // (via with_font_size, with_font_tier, or with_font)
  if (!result.font_size_explicitly_set &&
      default_font_name != UIComponent::UNSET_FONT) {
    result.font_size = default_font_size;
  }

  if (!defaults.has_value()) {
    return result;
  }

  return defaults.value().apply_overrides(result);
}

inline ComponentConfig _overwrite_defaults(HasUIContext auto &ctx,
                                           ComponentConfig config,
                                           ComponentType component_type,
                                           bool enable_color = false) {
  auto &styling_defaults = UIStylingDefaults::get();

  if (!config.is_internal) {
    if (styling_defaults.has_component_defaults(component_type)) {
      config = styling_defaults.merge_with_defaults(component_type, config);
    }
  }

  config.with_internal(true);

  // Button-specific defaults
  if (component_type == ComponentType::Button) {
    // Default size: 120x44 at 720p (44px height is touch target minimum)
    if (config.size.is_default) {
      config.with_size(ComponentSize{w1280(120), h720(44)});
    }
    // Default padding: Spacing::sm
    bool padding_is_default = config.padding.top.dim == Dim::None &&
                              config.padding.left.dim == Dim::None &&
                              config.padding.bottom.dim == Dim::None &&
                              config.padding.right.dim == Dim::None;
    if (padding_is_default) {
      config.with_padding(Spacing::sm);
    }
  }

  if (enable_color && config.color_usage == Theme::Usage::Default)
    config.with_color_usage(Theme::Usage::Primary);

  if (config.label_alignment == TextAlignment::None) {
    config.with_alignment(TextAlignment::Center);
  }

  if (!config.rounded_corners.has_value()) {
    config.with_rounded_corners(ctx.theme.rounded_corners);
  }
  if (!config.roundness.has_value()) {
    config.roundness = ctx.theme.roundness;
  }
  if (!config.segments.has_value()) {
    config.segments = ctx.theme.segments;
  }
  return config;
}

inline void apply_flags(Entity &entity, const ComponentConfig &config) {
  if (config.skip_when_tabbing)
    entity.addComponentIfMissing<SkipWhenTabbing>();
  if (config.select_on_focus)
    entity.addComponentIfMissing<SelectOnFocus>();
  if (config.has_click_activation_override()) {
    entity.addComponentIfMissing<HasClickActivationMode>(
        config.click_activation);
  } else {
    entity.removeComponentIfExists<HasClickActivationMode>();
  }

  if (config.hidden) {
    entity.addComponentIfMissing<ShouldHide>();
  } else {
    entity.removeComponentIfExists<ShouldHide>();
  }
}

inline void apply_layout(Entity &entity, const ComponentConfig &config) {
  entity.get<UIComponent>()
      .set_desired_width(config.size.x_axis)
      .set_desired_height(config.size.y_axis)
      .set_desired_padding(config.padding)
      .set_desired_margin(config.margin)
      .set_justify_content(config.justify_content)
      .set_align_items(config.align_items)
      .set_self_align(config.self_align)
      .set_flex_wrap(config.flex_wrap)
      .set_debug_wrap(config.debug_wrap)
      .set_flex_direction(config.flex_direction);

  if (config.is_absolute)
    entity.get<UIComponent>().make_absolute();
}

inline void apply_label(HasUIContext auto &ctx, Entity &entity,
                        const ComponentConfig &config) {
  if (config.label.empty())
    return;
  auto &lbl =
      entity.addComponentIfMissing<ui::HasLabel>(config.label, config.disabled);
  lbl.set_label(config.label)
      .set_disabled(config.disabled)
      .set_alignment(config.label_alignment);

  // Set explicit text color if specified via with_text_color()
  if (config.text_color_usage == Theme::Usage::Custom &&
      config.custom_text_color.has_value()) {
    lbl.set_explicit_text_color(config.custom_text_color.value());
  } else if (Theme::is_valid(config.text_color_usage)) {
    lbl.set_explicit_text_color(
        ctx.theme.from_usage(config.text_color_usage, config.disabled));
  } else {
    lbl.clear_explicit_text_color();
  }

  // Set background_hint for auto-contrast text color (Garnish integration)
  // Note: Check config directly since HasColor is added later in apply_config()
  if (config.auto_text_color) {
    if (config.color_usage == Theme::Usage::Custom &&
        config.custom_color.has_value()) {
      lbl.set_background_hint(config.custom_color.value());
    } else if (Theme::is_valid(config.color_usage)) {
      lbl.set_background_hint(ctx.theme.from_usage(config.color_usage));
    } else {
      lbl.set_background_hint(ctx.theme.background);
    }
  } else {
    lbl.clear_background_hint();
  }

  // Set text stroke if configured
  if (config.has_text_stroke()) {
    lbl.set_text_stroke(config.text_stroke_config.value());
  } else {
    lbl.clear_text_stroke();
  }

  // Set text shadow if configured
  if (config.has_text_shadow()) {
    lbl.set_text_shadow(config.text_shadow_config.value());
  } else {
    lbl.clear_text_shadow();
  }
}

inline void apply_texture(Entity &entity, const ComponentConfig &config) {
  if (!config.texture_config.has_value())
    return;
  const TextureConfig &conf = config.texture_config.value();
  auto &ht = entity.addComponentIfMissing<texture_manager::HasTexture>(
      conf.texture, conf.alignment);
  ht.texture = conf.texture;
  ht.alignment = conf.alignment;
}

inline void apply_shadow(Entity &entity, const ComponentConfig &config) {
  if (!config.has_shadow()) {
    entity.removeComponentIfExists<HasShadow>();
    return;
  }
  const Shadow &shadow = config.shadow_config.value();
  auto &hs = entity.addComponentIfMissing<HasShadow>(shadow);
  hs.shadow = shadow;
}

inline void apply_border(Entity &entity, const ComponentConfig &config) {
  if (!config.has_border()) {
    entity.removeComponentIfExists<HasBorder>();
    return;
  }
  const Border &border = config.border_config.value();
  auto &hb = entity.addComponentIfMissing<HasBorder>(border);
  hb.border = border;
}

inline void apply_bevel(Entity &entity, const ComponentConfig &config) {
  if (!config.has_bevel()) {
    entity.removeComponentIfExists<HasBevelBorder>();
    return;
  }
  const BevelBorder &bevel = config.bevel_config.value();
  entity.addComponentIfMissing<HasBevelBorder>(bevel);
}

inline void apply_render_layer(HasUIContext auto &ctx, Entity &entity,
                               Entity &parent, ComponentConfig &config) {
  // Inherit render layer from parent (child is at least on parent's layer)
  config.render_layer =
      std::max(config.render_layer, parent.get<UIComponent>().render_layer);
  entity.get<UIComponent>().render_layer = config.render_layer;
  ctx.queue_render(RenderInfo{entity.id, config.render_layer});
}

inline void apply_nine_slice(Entity &entity, const ComponentConfig &config) {
  if (!config.has_nine_slice()) {
    entity.removeComponentIfExists<HasNineSliceBorder>();
    return;
  }
  const NineSliceBorder &nine_slice = config.nine_slice_config.value();
  auto &hn = entity.addComponentIfMissing<HasNineSliceBorder>(nine_slice);
  hn.nine_slice = nine_slice;
}

inline void apply_visuals(HasUIContext auto &ctx, Entity &entity,
                          const ComponentConfig &config) {
  if (config.rounded_corners.has_value() &&
      config.rounded_corners.value().any()) {
    entity.addComponentIfMissing<HasRoundedCorners>()
        .set(config.rounded_corners.value())
        .set_roundness(config.roundness.value_or(0.5f))
        .set_segments(config.segments.value_or(8));
  }

  if (config.clip_children) {
    entity.addComponentIfMissing<HasClipChildren>();
  }

  if (config.font_name != UIComponent::UNSET_FONT) {
    entity.get<UIComponent>().enable_font(config.font_name, config.font_size);
  }

  if (Theme::is_valid(config.color_usage)) {
    entity.addComponentIfMissing<HasColor>(
        ctx.theme.from_usage(config.color_usage, config.disabled));
    entity.get<HasColor>().set(
        ctx.theme.from_usage(config.color_usage, config.disabled));
  } else if (config.color_usage == Theme::Usage::Custom) {
    if (config.custom_color.has_value()) {
      entity.addComponentIfMissing<HasColor>(config.custom_color.value());
      entity.get<HasColor>().set(config.custom_color.value());
    } else {
      entity.addComponentIfMissing<HasColor>(colors::UI_PINK);
      entity.get<HasColor>().set(colors::UI_PINK);
    }
  } else if (config.color_usage == Theme::Usage::Default &&
             !config.label.empty()) {
    // Auto-add transparent background for text-only elements so they
    // render correctly without requiring an explicit background color.
    entity.addComponentIfMissing<HasColor>(colors::transparent());
    entity.get<HasColor>().set(colors::transparent());
  }
  entity.addComponentIfMissing<HasOpacity>().value =
      std::clamp(config.opacity, 0.0f, 1.0f);
  // Apply UI modifiers (scale, translate) if any are non-default
  bool needs_modifiers = config.scale != 1.0f ||
                         config.translate_x.value != 0.0f ||
                         config.translate_y.value != 0.0f;
  if (needs_modifiers) {
    auto &mods = entity.addComponentIfMissing<HasUIModifiers>();
    // Apply scale (visual scaling after layout - smooth for animations)
    mods.scale = config.scale;
    // Resolve Size to pixels using screen height (default 720p baseline)
    float screen_height = 720.f;
    if (auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>()) {
      screen_height = static_cast<float>(pcr->current_resolution.height);
    }
    mods.translate_x = resolve_to_pixels(config.translate_x, screen_height);
    mods.translate_y = resolve_to_pixels(config.translate_y, screen_height);
  } else {
    // Reset modifiers if component exists but no modifiers needed
    if (entity.has<HasUIModifiers>()) {
      auto &mods = entity.get<HasUIModifiers>();
      mods.scale = 1.0f;
      mods.translate_x = 0.0f;
      mods.translate_y = 0.0f;
    }
  }
}

inline void apply_animations(HasUIContext auto &ctx, Entity &entity,
                             const ComponentConfig &config) {
  if (config.animations.empty())
    return;

  auto &state = entity.addComponentIfMissing<HasAnimationState>();
  // Clamp dt to prevent huge jumps on first frame or after pauses
  float dt = std::min(ctx.dt, 1.0f / 200.0f);

  // Handle OnAppear trigger (first time we see this entity with animations)
  if (!state.has_appeared) {
    state.has_appeared = true;
    for (const auto &anim_def : config.animations) {
      if (anim_def.trigger == AnimTrigger::OnAppear) {
        auto &track = state.get(anim_def.property);
        anim::start(track, anim_def.from_value, anim_def.to_value);
      }
    }
    // Skip update on first frame - let it render at from_value first
    goto apply_values;
  }

  // Process each animation definition
  for (const auto &anim_def : config.animations) {
    auto &track = state.get(anim_def.property);

    // OnAppear: already started above, just update
    if (anim_def.trigger == AnimTrigger::OnAppear) {
      anim::update(track, anim_def, dt);
      continue;
    }

    // Loop: ping-pong when animation completes
    if (anim_def.trigger == AnimTrigger::Loop) {
      if (!track.is_active) {
        float next_from = track.target;
        float next_to = (track.target == anim_def.to_value)
                            ? anim_def.from_value
                            : anim_def.to_value;
        anim::start(track, next_from, next_to);
      }
      anim::update(track, anim_def, dt);
      continue;
    }

    // Edge-triggered animations: OnClick, OnHover, OnFocus
    // Use was_hot/was_active since current frame's state isn't set until
    // HandleClicks runs after screen rendering
    bool trigger_active = false;
    switch (anim_def.trigger) {
    case AnimTrigger::OnClick:
      trigger_active = ctx.was_active(entity.id);
      break;
    case AnimTrigger::OnHover:
      trigger_active = ctx.was_hot(entity.id);
      break;
    case AnimTrigger::OnFocus:
      trigger_active = ctx.has_focus(entity.id);
      break;
    default:
      break;
    }

    // Start on rising edge, reverse on falling edge
    if (trigger_active && !track.triggered) {
      anim::start_to(track, anim_def.to_value);
    } else if (!trigger_active && track.triggered) {
      anim::start_to(track, anim_def.from_value);
    }
    track.triggered = trigger_active;

    anim::update(track, anim_def, dt);
  }

apply_values:
  // Apply animated values to modifiers (additive)
  auto &mods = entity.addComponentIfMissing<HasUIModifiers>();

  // Scale is multiplicative
  if (state.scale.is_active || state.scale.current != 1.0f) {
    mods.scale *= state.scale.current;
  }

  // Translate is additive
  mods.translate_x += state.translate_x.current;
  mods.translate_y += state.translate_y.current;

  // Rotation is additive (in degrees)
  mods.rotation += state.rotation.current;

  // Opacity (apply to HasOpacity component)
  if (state.opacity.is_active || state.opacity.current != 1.0f) {
    if (entity.has<HasOpacity>()) {
      entity.get<HasOpacity>().value *= state.opacity.current;
    }
  }
}

inline bool _add_missing_components(HasUIContext auto &ctx, Entity &entity,
                                    Entity &parent, ComponentConfig config,
                                    const std::string &debug_name = "") {
  (void)debug_name;
  bool created = false;

  if (entity.is_missing<UIComponent>()) {
    entity.addComponent<ui::UIComponent>(entity.id).set_parent(parent.id);
    entity.addComponent<UIComponentDebug>(debug_name);

    created = true;
  }

  if (!config.debug_name.empty()) {
    entity.get<UIComponentDebug>().set(config.debug_name);
  } else if (!config.label.empty()) {
    // Auto-derive debug name from label: "<label> <component_type>"
    entity.get<UIComponentDebug>().set(
        debug_name.empty() ? config.label
                           : fmt::format("{} {}", config.label, debug_name));
  }

  UIComponent &parent_cmp = parent.get<UIComponent>();
  parent_cmp.add_child(entity.id);

  apply_flags(entity, config);
  apply_layout(entity, config);
  apply_visuals(ctx, entity, config);
  apply_animations(ctx, entity, config);
  apply_label(ctx, entity, config);
  apply_texture(entity, config);
  apply_shadow(entity, config);
  apply_border(entity, config);
  apply_bevel(entity, config);
  apply_nine_slice(entity, config);
  apply_render_layer(ctx, entity, parent, config);
  return created;
}

// Validate ComponentConfig for common issues.
// Only runs when validation mode is Warn or Strict.
inline void _validate_config(const ComponentConfig &config,
                             const std::string &debug_name) {
  const auto &validation = UIStylingDefaults::get().get_validation_config();
  if (validation.mode == ValidationMode::Silent)
    return;

  auto warn = [&](const std::string &msg) {
    std::string name =
        !config.debug_name.empty()
            ? config.debug_name
            : (!debug_name.empty() ? debug_name : "<unnamed>");
    std::string full = "[UI Config] " + name + ": " + msg;
    if (validation.mode == ValidationMode::Strict) {
      log_error("{}", full);
    } else {
      log_warn("{}", full);
    }
  };

  // Warn: fill_parent on both axes without explicit parent size hint
  // (This is the most common gotcha for new users)
  if (config.size.x_axis.dim == Dim::Percent &&
      config.size.x_axis.value >= 1.0f && config.size.y_axis.dim == Dim::Percent &&
      config.size.y_axis.value >= 1.0f && config.is_absolute) {
    warn("fill_parent() with absolute positioning may not reference "
         "the expected parent. Consider using explicit pixel sizes.");
  }

  // Warn: text without any font specified and no global default
  if (!config.label.empty() && !config.has_font_override()) {
    auto &defaults = UIStylingDefaults::get();
    if (defaults.default_font_name == UIComponent::UNSET_FONT) {
      warn("Text element has no font and no global default font is set. "
           "Call UIStylingDefaults::get().set_default_font() or use "
           ".with_font().");
    }
  }
}

inline bool _init_component(HasUIContext auto &ctx, EntityParent ep_pair,
                            ComponentConfig &config,
                            ComponentType component_type,
                            bool enable_color = false,
                            const std::string &debug_name = "") {
  auto [entity, parent] = ep_pair;
  config = _overwrite_defaults(ctx, config, component_type, enable_color);
  _validate_config(config, debug_name);
  return _add_missing_components(ctx, entity, parent, config, debug_name);
}

template <typename Component, typename... CArgs>
Component &_init_state(Entity &entity,
                       const std::function<void(Component &)> &cb,
                       CArgs &&...args) {
  auto &cmp =
      entity.addComponentIfMissing<Component>(std::forward<CArgs>(args)...);
  cb(cmp);
  return cmp;
}

} // namespace imm

} // namespace ui

} // namespace afterhours
