#pragma once

#include <format>
#include <functional>
#include <optional>
#include <string>

#include "../../../drawing_helpers.h"
#include "../../../entity.h"
#include "../../../entity_helper.h"
#include "../../../logging.h"
#include "../../autolayout.h"
#include "../../color.h"
#include "../../texture_manager.h"
#include "../components.h"
#include "../context.h"
#include "../theme.h"
#include "element_result.h"
#include "entity_management.h"
#include "rounded_corners.h"

namespace afterhours {

namespace ui {

namespace imm {

static Vector2Type default_component_size = {200.f, 50.f};

struct TextureConfig {
  texture_manager::Texture texture;
  texture_manager::HasTexture::Alignment alignment =
      texture_manager::HasTexture::Alignment::None;
};

struct ComponentConfig {
  ComponentSize size = ComponentSize(pixels(default_component_size.x),
                                     pixels(default_component_size.y), true);
  Padding padding;
  Margin margin;
  std::string label;
  bool is_absolute = false;
  FlexDirection flex_direction = FlexDirection::Column;

  Theme::Usage color_usage = Theme::Usage::Default;
  std::optional<Color> custom_color;

  std::optional<TextureConfig> texture_config;
  std::optional<texture_manager::HasTexture::Alignment> image_alignment;
  std::optional<std::bitset<4>> rounded_corners;

  // TODO should everything be inheritable?
  // inheritable options
  TextAlignment label_alignment = TextAlignment::None;
  bool skip_when_tabbing = false;
  bool disabled = false;
  bool hidden = false;
  bool select_on_focus = false;

  // debugs
  std::string debug_name = "";
  int render_layer = 0;

  std::string font_name = UIComponent::UNSET_FONT;
  float font_size = 50.f;
  bool is_internal = false;

  ComponentConfig &with_label(const std::string &lbl) {
    label = lbl;
    return *this;
  }
  ComponentConfig &with_size(const ComponentSize &sz) {
    size = sz;
    return *this;
  }
  ComponentConfig &with_padding(const Padding &padding_) {
    padding = padding_;
    return *this;
  }
  ComponentConfig &with_margin(const Margin &margin_) {
    margin = margin_;
    return *this;
  }
  ComponentConfig &with_color_usage(Theme::Usage usage) {
    color_usage = usage;
    return *this;
  }
  ComponentConfig &with_custom_color(Color color) {
    color_usage = Theme::Usage::Custom;
    custom_color = color;
    return *this;
  }
  ComponentConfig &with_alignment(TextAlignment align) {
    label_alignment = align;
    return *this;
  }
  ComponentConfig &with_rounded_corners(const std::bitset<4> &corners) {
    rounded_corners = corners;
    return *this;
  }
  ComponentConfig &with_rounded_corners(const RoundedCorners &corners) {
    rounded_corners = corners.get();
    return *this;
  }
  ComponentConfig &disable_rounded_corners() {
    rounded_corners = std::bitset<4>().reset();
    return *this;
  }
  ComponentConfig &with_debug_name(const std::string &name) {
    debug_name = name;
    return *this;
  }
  ComponentConfig &with_render_layer(int layer) {
    render_layer = layer;
    return *this;
  }
  ComponentConfig &with_disabled(bool dis) {
    disabled = dis;
    return *this;
  }
  ComponentConfig &with_hidden(bool hide) {
    hidden = hide;
    return *this;
  }
  ComponentConfig &with_skip_tabbing(bool skip) {
    skip_when_tabbing = skip;
    return *this;
  }
  ComponentConfig &with_select_on_focus(bool select) {
    select_on_focus = select;
    return *this;
  }
  ComponentConfig &with_flex_direction(FlexDirection dir) {
    flex_direction = dir;
    return *this;
  }
  ComponentConfig &with_font(const std::string &font_name_, float font_size_) {
    font_name = font_name_;
    font_size = font_size_;
    return *this;
  }
  ComponentConfig &with_absolute_position() {
    is_absolute = true;
    return *this;
  }
  ComponentConfig &with_internal(bool internal = true) {
    is_internal = internal;
    return *this;
  }
  ComponentConfig &with_texture(const TextureConfig &tex_cfg) {
    texture_config = tex_cfg;
    return *this;
  }
  ComponentConfig &
  with_texture(texture_manager::Texture texture,
               texture_manager::HasTexture::Alignment alignment =
                   texture_manager::HasTexture::Alignment::None) {
    texture_config = TextureConfig{texture, alignment};
    return *this;
  }

  ComponentConfig &
  with_image_alignment(texture_manager::HasTexture::Alignment alignment) {
    image_alignment = alignment;
    return *this;
  }

  bool has_padding() const {
    return padding.top.value > 0 || padding.left.value > 0 ||
           padding.bottom.value > 0 || padding.right.value > 0;
  }
  bool has_margin() const {
    return margin.top.value > 0 || margin.left.value > 0 ||
           margin.bottom.value > 0 || margin.right.value > 0;
  }
  bool has_size_override() const { return !size.is_default; }
  bool has_label_alignment_override() const {
    return label_alignment != TextAlignment::None;
  }
  bool has_any_rounded_corners() const { return rounded_corners.has_value(); }
  bool has_font_override() const {
    return font_name != UIComponent::UNSET_FONT;
  }
  bool has_texture() const { return texture_config.has_value(); }
  bool has_image_alignment() const { return image_alignment.has_value(); }
  bool is_disabled() const { return disabled; }
  bool is_hidden() const { return hidden; }
  bool skips_when_tabbing() const { return skip_when_tabbing; }
  bool selects_on_focus() const { return select_on_focus; }

  // Static method to create inheritable config from parent
  static ComponentConfig inherit_from(const ComponentConfig &parent,
                                      const std::string &debug_name = "") {
    return ComponentConfig{}
        .with_debug_name(debug_name)
        .apply_inheritable_from(parent);
  }

  // Copies only inheritable fields from parent into this config
  ComponentConfig &apply_inheritable_from(const ComponentConfig &parent) {
    label_alignment = parent.label_alignment;
    disabled = parent.disabled;
    hidden = parent.hidden;
    skip_when_tabbing = parent.skip_when_tabbing;
    select_on_focus = parent.select_on_focus;
    font_name = parent.font_name;
    font_size = parent.font_size;
    is_internal = parent.is_internal;
    image_alignment = parent.image_alignment.value_or(
        texture_manager::HasTexture::Alignment::Center);
    return *this;
  }
};

struct ComponentConfigBuilder : public ComponentConfig {
  ComponentConfigBuilder() = default;
  ComponentConfigBuilder(const ComponentConfig &config)
      : ComponentConfig(config) {}

  ComponentConfig build() const { return *this; }
};

// Component type enum for styling defaults
enum struct ComponentType {
  Button,
  ButtonGroup,
  Div,
  Slider,
  Checkbox,
  CheckboxNoLabel,
  Dropdown,
  Pagination,
  NavigationBar,
  CheckboxGroup,
  Image,
};

// TODO singleton helper
struct UIStylingDefaults {
  std::map<ComponentType, ComponentConfig> component_configs;

  UIStylingDefaults() = default;

  // Singleton pattern
  static UIStylingDefaults &get() {
    static UIStylingDefaults instance;
    return instance;
  }

  // Delete copy constructor and assignment operator
  UIStylingDefaults(const UIStylingDefaults &) = delete;
  UIStylingDefaults &operator=(const UIStylingDefaults &) = delete;

  // Set default styling with real ComponentConfig
  UIStylingDefaults &set_component_config(ComponentType component_type,
                                          const ComponentConfig &config) {
    component_configs[component_type] = config;
    return *this;
  }

  // Get real ComponentConfig for a component type
  std::optional<ComponentConfig>
  get_component_config(ComponentType component_type) const {
    auto it = component_configs.find(component_type);
    if (it != component_configs.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  // Check if defaults exist for a component type
  bool has_component_defaults(ComponentType component_type) const {
    return component_configs.find(component_type) != component_configs.end();
  }

  // Merge component defaults with a config
  ComponentConfig merge_with_defaults(ComponentType component_type,
                                      const ComponentConfig &config) const {
    auto defaults = get_component_config(component_type);
    if (!defaults.has_value()) {
      return config;
    }

    ComponentConfig merged = defaults.value();

    if (config.has_padding())
      merged.padding = config.padding;
    if (config.has_margin())
      merged.margin = config.margin;
    if (config.has_size_override())
      merged.size = config.size;

    if (config.color_usage != Theme::Usage::Default) {
      merged.color_usage = config.color_usage;
      merged.custom_color = config.custom_color;
    }

    if (config.has_label_alignment_override())
      merged.label_alignment = config.label_alignment;

    if (!config.label.empty())
      merged.label = config.label;

    if (config.has_any_rounded_corners())
      merged.rounded_corners = config.rounded_corners;

    if (config.is_disabled())
      merged.disabled = config.disabled;
    if (config.is_hidden())
      merged.hidden = config.hidden;
    if (config.skips_when_tabbing())
      merged.skip_when_tabbing = config.skip_when_tabbing;
    if (config.selects_on_focus())
      merged.select_on_focus = config.select_on_focus;

    if (config.has_font_override()) {
      merged.font_name = config.font_name;
      merged.font_size = config.font_size;
    }

    if (config.has_texture())
      merged.texture_config = config.texture_config;
    if (config.has_image_alignment())
      merged.image_alignment = config.image_alignment;

    if (config.is_absolute)
      merged.is_absolute = config.is_absolute;
    if (config.flex_direction != FlexDirection::Column)
      merged.flex_direction = config.flex_direction;
    if (config.render_layer != 0)
      merged.render_layer = config.render_layer;
    if (!config.debug_name.empty())
      merged.debug_name = config.debug_name;

    return merged;
  }
};

template <typename T>
concept HasUIContext = requires(T a) {
  {
    std::is_same_v<T, UIContext<typename decltype(a)::value_type>>
  } -> std::convertible_to<bool>;
};

ComponentConfig _overwrite_defaults(HasUIContext auto &ctx,
                                    ComponentConfig config,
                                    ComponentType component_type,
                                    bool enable_color = false) {
  auto &styling_defaults = UIStylingDefaults::get();

  if (!config.is_internal &&
      styling_defaults.has_component_defaults(component_type)) {
    config = styling_defaults.merge_with_defaults(component_type, config);
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
  return config;
}

static bool _init_component(HasUIContext auto &ctx, EntityParent ep_pair,
                            ComponentConfig &config,
                            ComponentType component_type,
                            bool enable_color = false,
                            const std::string &debug_name = "") {
  auto [entity, parent] = ep_pair;
  config = _overwrite_defaults(ctx, config, component_type, enable_color);
  return _add_missing_components(ctx, entity, parent, config, debug_name);
}

static bool _add_missing_components(HasUIContext auto &ctx, Entity &entity,
                                    Entity &parent, ComponentConfig config,
                                    const std::string &debug_name = "") {
  (void)debug_name;
  bool created = false;

  if (entity.is_missing<UIComponent>()) {
    entity.addComponent<ui::UIComponent>(entity.id).set_parent(parent.id);

    entity.addComponent<UIComponentDebug>(debug_name);

    if (!config.label.empty())
      entity.addComponent<ui::HasLabel>(config.label, config.disabled)
          .set_alignment(config.label_alignment);

    if (Theme::is_valid(config.color_usage)) {
      entity.addComponent<HasColor>(
          ctx.theme.from_usage(config.color_usage, config.disabled));

      if (config.custom_color.has_value()) {
        log_warn("You have custom color set on {} but didnt set "
                 "config.color_usage = Custom",
                 debug_name.c_str());
      }
    }

    if (config.color_usage == Theme::Usage::Custom) {
      if (config.custom_color.has_value()) {
        entity.addComponentIfMissing<HasColor>(config.custom_color.value());
      } else {
        log_warn("You have custom color usage selected on {} but didnt set "
                 "config.custom_color",
                 debug_name.c_str());
        entity.addComponentIfMissing<HasColor>(colors::UI_PINK);
      }
    }

    if (config.skip_when_tabbing)
      entity.addComponent<SkipWhenTabbing>();

    if (config.select_on_focus)
      entity.addComponent<SelectOnFocus>();

    if (config.texture_config.has_value()) {
      const TextureConfig &conf = config.texture_config.value();
      entity.addComponent<texture_manager::HasTexture>(conf.texture,
                                                       conf.alignment);
    }

    created = true;
  }

  UIComponent &parent_cmp = parent.get<UIComponent>();
  parent_cmp.add_child(entity.id);

  if (config.hidden) {
    entity.addComponentIfMissing<ShouldHide>();
  } else {
    entity.removeComponentIfExists<ShouldHide>();
  }

  entity.get<UIComponent>()
      .set_desired_width(config.size.x_axis)
      .set_desired_height(config.size.y_axis)
      .set_desired_padding(config.padding)
      .set_desired_margin(config.margin);

  if (config.rounded_corners.has_value() &&
      config.rounded_corners.value().any()) {
    entity.addComponentIfMissing<HasRoundedCorners>().set(
        config.rounded_corners.value());
  }

  if (!config.label.empty())
    entity.get<ui::HasLabel>()
        .set_label(config.label)
        .set_disabled(config.disabled)
        .set_alignment(config.label_alignment);

  if (config.is_absolute)
    entity.get<UIComponent>().make_absolute();

  if (!config.debug_name.empty()) {
    entity.get<UIComponentDebug>().set(config.debug_name);
  }

  if (config.font_name != UIComponent::UNSET_FONT) {
    entity.get<UIComponent>().enable_font(config.font_name, config.font_size);
  }

  if (Theme::is_valid(config.color_usage)) {
    entity.get<HasColor>().set(
        ctx.theme.from_usage(config.color_usage, config.disabled));
  }

  if (config.color_usage == Theme::Usage::Custom) {
    if (config.custom_color.has_value()) {
      entity.get<HasColor>().set(config.custom_color.value());
    } else {
      entity.get<HasColor>().set(colors::UI_PINK);
    }
  }

  if (!config.label.empty())
    entity.get<ui::HasLabel>().label = config.label;

  if (config.texture_config.has_value()) {
    const TextureConfig &conf = config.texture_config.value();
    auto &ht = entity.addComponentIfMissing<texture_manager::HasTexture>(
        conf.texture, conf.alignment);
    ht.texture = conf.texture;
    ht.alignment = conf.alignment;
  }

  ctx.queue_render(RenderInfo{entity.id, config.render_layer});
  return created;
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