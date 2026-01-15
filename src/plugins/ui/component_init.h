#pragma once

#include <functional>

#include "../../ecs.h"
#include "../color.h"
#include "../texture_manager.h"
#include "component_config.h"
#include "components.h"
#include "context.h"
#include "entity_management.h"

namespace afterhours {

namespace ui {

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

  if (result.font_name == UIComponent::UNSET_FONT &&
      default_font_name != UIComponent::UNSET_FONT) {
    result.font_name = default_font_name;
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

inline void apply_visuals(HasUIContext auto &ctx, Entity &entity,
                          const ComponentConfig &config) {
  if (config.rounded_corners.has_value() &&
      config.rounded_corners.value().any()) {
    entity.addComponentIfMissing<HasRoundedCorners>()
        .set(config.rounded_corners.value())
        .set_roundness(config.roundness.value_or(0.5f))
        .set_segments(config.segments.value_or(8));
  }

  if (config.font_name != UIComponent::UNSET_FONT) {
    float effective_font_size = config.font_size;

#ifdef AFTERHOURS_ENFORCE_MIN_FONT_SIZE
    // Warn and enforce minimum accessible font size
    if (config.font_size < TypographyScale::MIN_ACCESSIBLE_SIZE_720P) {
      log_warn("Font size {:.1f}px is below minimum accessible size "
               "{:.1f}px for component '{}' - clamping to minimum",
               config.font_size, TypographyScale::MIN_ACCESSIBLE_SIZE_720P,
               config.debug_name.empty() ? "(unnamed)" : config.debug_name);
      effective_font_size = TypographyScale::MIN_ACCESSIBLE_SIZE_720P;
    }
#endif
    entity.get<UIComponent>().enable_font(config.font_name,
                                          effective_font_size);
  }

  if (Theme::is_valid(config.color_usage)) {
    entity.addComponentIfMissing<HasColor>(
        ctx.theme.from_usage(config.color_usage, config.disabled));
    entity.get<HasColor>().set(
        ctx.theme.from_usage(config.color_usage, config.disabled));
  }

  if (config.color_usage == Theme::Usage::Custom) {
    if (config.custom_color.has_value()) {
      entity.addComponentIfMissing<HasColor>(config.custom_color.value());
      entity.get<HasColor>().set(config.custom_color.value());
    } else {
      entity.addComponentIfMissing<HasColor>(colors::UI_PINK);
      entity.get<HasColor>().set(colors::UI_PINK);
    }
  }
  entity.addComponentIfMissing<HasOpacity>().value =
      std::clamp(config.opacity, 0.0f, 1.0f);
  if (config.translate_x != 0.0f || config.translate_y != 0.0f) {
    auto &mods = entity.addComponentIfMissing<HasUIModifiers>();
    mods.translate_x = config.translate_x;
    mods.translate_y = config.translate_y;
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
  }

  UIComponent &parent_cmp = parent.get<UIComponent>();
  parent_cmp.add_child(entity.id);

  apply_flags(entity, config);
  apply_layout(entity, config);
  apply_visuals(ctx, entity, config);
  apply_label(ctx, entity, config);
  apply_texture(entity, config);
  apply_shadow(entity, config);
  apply_border(entity, config);

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});
  return created;
}

inline bool _init_component(HasUIContext auto &ctx, EntityParent ep_pair,
                            ComponentConfig &config,
                            ComponentType component_type,
                            bool enable_color = false,
                            const std::string &debug_name = "") {
  auto [entity, parent] = ep_pair;
  config = _overwrite_defaults(ctx, config, component_type, enable_color);
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
