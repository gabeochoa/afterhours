#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <string>

#include "../../ecs.h"
#include "../input_system.h"
#include "component_init.h"
#include "components.h"
#include "element_result.h"
#include "entity_management.h"
#include "fmt/format.h"
#include "rendering.h"
#include "rounded_corners.h"
#include "text_input_utils.h"

namespace afterhours {

namespace ui {

namespace imm {

inline size_t prev_index(size_t current, size_t total) {
  return (current == 0) ? total - 1 : current - 1;
}

inline size_t next_index(size_t current, size_t total) {
  return (current + 1) % total;
}

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (config.size.is_default && config.label.empty())
    config.with_size(ComponentSize{children(), children()});
  if (config.size.is_default && !config.label.empty())
    config.with_size(ComponentSize{children(default_component_size.x),
                                   children(default_component_size.y)});

  _init_component(ctx, ep_pair, config, ComponentType::Div);

  return {true, entity};
}

/// Orientation for separator widgets
enum struct SeparatorOrientation {
  Horizontal, // Thin horizontal line (default)
  Vertical,   // Thin vertical line
};

/// Creates a visual separator line between UI sections.
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param orientation Horizontal (thin height) or Vertical (thin width)
/// @param config Component configuration
///
/// Features:
/// - Horizontal line by default (fills parent width, thin height)
/// - Vertical orientation available
/// - Uses Theme::Usage::Secondary by default for subtle appearance
/// - Optional label creates "--- Label ---" style separator
///
/// Usage:
/// ```cpp
/// // Simple horizontal separator
/// separator(ctx, mk(parent));
///
/// // Vertical separator
/// separator(ctx, mk(parent), SeparatorOrientation::Vertical);
///
/// // Labeled separator (section divider)
/// separator(ctx, mk(parent), SeparatorOrientation::Horizontal,
///           ComponentConfig{}.with_label("Settings"));
/// ```
ElementResult
separator(HasUIContext auto &ctx, EntityParent ep_pair,
          SeparatorOrientation orientation = SeparatorOrientation::Horizontal,
          ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Use styling defaults if available, otherwise use resolution-scaled default
  // Default: 1/4 of tiny spacing (8px/4 = 2px at 720p baseline)
  auto &styling_defaults = UIStylingDefaults::get();
  Size separator_thickness = h720(DefaultSpacing::tiny().value * 0.25f);

  if (auto def =
          styling_defaults.get_component_config(ComponentType::Separator);
      def.has_value()) {
    // Use configured thickness from styling defaults
    if (!def->size.is_default) {
      separator_thickness = orientation == SeparatorOrientation::Horizontal
                                ? def->size.y_axis
                                : def->size.x_axis;
    }
  }

  // Set default size based on orientation
  if (config.size.is_default) {
    if (orientation == SeparatorOrientation::Horizontal) {
      // Horizontal: fill width, thin height
      config.with_size(ComponentSize{percent(1.0f), separator_thickness});
    } else {
      // Vertical: thin width, fill height
      config.with_size(ComponentSize{separator_thickness, percent(1.0f)});
    }
  }

  // Default to Secondary color for subtle appearance if not specified
  if (config.color_usage == Theme::Usage::Default) {
    config.with_background(Theme::Usage::Secondary);
  }

  // Add small default margin if none specified
  if (!config.has_margin()) {
    if (orientation == SeparatorOrientation::Horizontal) {
      config.with_margin(Margin{.top = DefaultSpacing::small(),
                                .bottom = DefaultSpacing::small()});
    } else {
      config.with_margin(Margin{.left = DefaultSpacing::small(),
                                .right = DefaultSpacing::small()});
    }
  }

  // If there's a label, create a labeled separator: [line] Label [line]
  if (!config.label.empty()) {
    std::string label_text = config.label;
    config.label = ""; // Clear label from main container

    // Container should use Row layout for horizontal, Column for vertical
    config.with_flex_direction(orientation == SeparatorOrientation::Horizontal
                                   ? FlexDirection::Row
                                   : FlexDirection::Column);
    config.with_background(Theme::Usage::None);

    // Adjust container size to fit content
    if (orientation == SeparatorOrientation::Horizontal) {
      config.with_size(ComponentSize{percent(1.0f), children()});
    } else {
      config.with_size(ComponentSize{children(), percent(1.0f)});
    }

    _init_component(ctx, ep_pair, config, ComponentType::Separator, false,
                    "separator_labeled");

    // Create line - label - line structure
    auto line_size = orientation == SeparatorOrientation::Horizontal
                         ? ComponentSize{percent(0.3f), separator_thickness}
                         : ComponentSize{separator_thickness, percent(0.3f)};

    // First line
    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "separator_line_1")
            .with_size(line_size)
            .with_background(Theme::Usage::Secondary)
            .with_margin(Margin{}));

    // Label in the middle
    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "separator_label")
            .with_size(ComponentSize{children(), children()})
            .with_label(label_text)
            .with_background(Theme::Usage::None)
            .with_padding(Padding{.left = DefaultSpacing::small(),
                                  .right = DefaultSpacing::small()})
            .with_margin(Margin{}));

    // Second line
    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "separator_line_2")
            .with_size(line_size)
            .with_background(Theme::Usage::Secondary)
            .with_margin(Margin{}));

    return {false, entity};
  }

  // Simple separator line (no label)
  config.with_skip_tabbing(true); // Separators shouldn't be focusable
  _init_component(ctx, ep_pair, config, ComponentType::Separator, false,
                  "separator");

  return {false, entity};
}

ElementResult image(HasUIContext auto &ctx, EntityParent ep_pair,
                    ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image, false, "image");

  return {false, entity};
}

ElementResult sprite(HasUIContext auto &ctx, EntityParent ep_pair,
                     afterhours::texture_manager::Texture texture,
                     afterhours::texture_manager::Rectangle source_rect,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image, false, "sprite");

  auto alignment = config.image_alignment.value_or(
      afterhours::texture_manager::HasTexture::Alignment::Center);
  auto &img = entity.addComponentIfMissing<ui::HasImage>(texture, source_rect,
                                                         alignment);
  img.texture = texture;
  img.source_rect = source_rect;
  img.alignment = alignment;

  return {false, entity};
}

inline ElementResult
image_button(HasUIContext auto &ctx, EntityParent ep_pair,
             afterhours::texture_manager::Texture texture,
             afterhours::texture_manager::Rectangle source_rect,
             ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image, true,
                  "image_button");

  auto alignment = config.image_alignment.value_or(
      afterhours::texture_manager::HasTexture::Alignment::Center);
  auto &img = entity.addComponentIfMissing<ui::HasImage>(texture, source_rect,
                                                         alignment);
  img.texture = texture;
  img.source_rect = source_rect;
  img.alignment = alignment;

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});
  return ElementResult{entity.get<HasClickListener>().down, entity};
}

template <typename Container>
ElementResult icon_row(HasUIContext auto &ctx, EntityParent ep_pair,
                       afterhours::texture_manager::Texture spritesheet,
                       const Container &frames, float scale = 1.f,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto row =
      div(ctx, ep_pair,
          ComponentConfig::inherit_from(config, "icon_row")
              .with_size(config.size)
              .with_margin(config.margin)
              .with_padding(config.padding)
              .with_flex_direction(FlexDirection::Row)
              .with_debug_name(config.debug_name.empty() ? "icon_row"
                                                         : config.debug_name));

  size_t i = 0;
  for (const auto &frame : frames) {
    auto icon_width = pixels(frame.width * scale);
    auto icon_height = pixels(frame.height * scale);

    sprite(ctx, mk(row.ent(), static_cast<int>(i)), spritesheet, frame,
           ComponentConfig::inherit_from(config)
               .with_image_alignment(
                   afterhours::texture_manager::HasTexture::Alignment::Center)
               .with_size(ComponentSize{icon_width, icon_height})
               .with_debug_name(fmt::format("icon_row_item_{}", i)));
    i++;
  }

  return {false, row.ent()};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Button, true, "button");

  // Apply flex-direction specifically for buttons so they can drive wrapping
  // TODO: this is a hack to get buttons to wrap. We should find a better way
  // to do this.
  entity.get<UIComponent>().flex_direction = config.flex_direction;

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

template <typename Container>
ElementResult button_group(HasUIContext auto &ctx, EntityParent ep_pair,
                           const Container &labels,
                           ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto max_height = config.size.y_axis;
  config.size.y_axis = children(max_height.value);
  auto max_width = config.size.x_axis;
  config.size.x_axis = children(max_width.value);

  _init_component(ctx, ep_pair, config, ComponentType::ButtonGroup, false,
                  "button_group");

  config.size.x_axis = config.flex_direction == FlexDirection::Row
                           ? pixels(max_width.value / labels.size())
                           : max_width;
  config.size.y_axis = config.flex_direction == FlexDirection::Row
                           ? max_height
                           : children(max_height.value);

  entity.get<UIComponent>().flex_direction = config.flex_direction;

  bool clicked = false;
  int value = -1;
  for (size_t i = 0; i < labels.size(); ++i) {
    if (button(
            ctx, mk(entity, i),
            ComponentConfig::inherit_from(config,
                                          fmt::format("button group {}", i))
                .with_size(config.size)
                .with_label(i < labels.size() ? std::string(labels[i]) : ""))) {
      clicked = true;
      value = static_cast<int>(i);
    }
  }

  return {clicked, entity, value};
}

ElementResult checkbox_no_label(HasUIContext auto &ctx, EntityParent ep_pair,
                                bool &value,
                                ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  HasCheckboxState &checkboxState =
      _init_state<HasCheckboxState>(entity, [&](auto &) {}, value);

  config.label = value ? "X" : " ";
  // Only set symbol font if no font override was specified
  // Preserve the inherited font_size for accessibility compliance
  if (!config.has_font_override()) {
    config.font_name = UIComponent::SYMBOL_FONT;
    config.font_size = 20.f; // Use accessible minimum size
  }

  _init_component(ctx, ep_pair, config, ComponentType::CheckboxNoLabel, true,
                  "checkbox");

  if (config.disabled) {
    entity.removeComponentIfExists<HasClickListener>();
  } else {
    entity.addComponentIfMissing<HasClickListener>([](Entity &ent) {
      auto &cbs = ent.get<HasCheckboxState>();
      cbs.on = !cbs.on;
      cbs.changed_since = true;
    });
  }

  value = checkboxState.on;
  ElementResult result{checkboxState.changed_since, entity, value};
  checkboxState.changed_since = false;
  return result;
}

ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                       bool &value,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto label = config.label;
  config.label = "";

  _init_component(ctx, ep_pair, config, ComponentType::Div, false,
                  "checkbox_row");

  // 2025-08-11: ensure checkbox row uses responsive defaults so both label
  // and button scale with resolution. Previously, only the label used a
  // responsive size, causing the button to remain tiny at higher
  // DPIs/resolutions.
  {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def =
            styling_defaults.get_component_config(ComponentType::Checkbox);
        def.has_value()) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(pixels(default_component_size.x),
                                  children(default_component_size.y));
    }
  }
  bool has_label_child = !label.empty();
  // Check if user provided custom corner configuration
  bool user_specified_corners = config.rounded_corners.has_value();

  if (has_label_child) {
    config.size = config.size._scale_x(0.5f);

    auto label_config =
        ComponentConfig::inherit_from(
            config, fmt::format("checkbox label {}", config.debug_name))
            .with_size(config.size)
            .with_label(label);

    // Apply default styling only if user hasn't specified custom settings
    if (config.color_usage == Theme::Usage::Default) {
      label_config.with_color_usage(Theme::Usage::Primary);
      // Only apply default corners if user didn't specify their own
      if (!user_specified_corners) {
        label_config.with_rounded_corners(RoundedCorners().right_sharp());
      }
    }

    div(ctx, mk(entity), label_config);
  }

  // 2025-08-11: explicitly propagate the responsive size to the clickable
  // checkbox so it scales along with the label and row container.
  auto checkbox_config =
      ComponentConfig::inherit_from(
          config, fmt::format("checkbox indiv from {}", config.debug_name))
          .with_size(config.size);

  if (config.color_usage == Theme::Usage::Default) {
    checkbox_config.with_color_usage(Theme::Usage::Primary);
    // Only apply default corners if user didn't specify their own
    if (!user_specified_corners) {
      checkbox_config.with_rounded_corners(RoundedCorners().left_sharp());
    }
  }

  bool changed = false;
  auto checkbox_ent =
      checkbox_no_label(ctx, mk(entity), value, checkbox_config);
  // Focus ring is drawn on the actual clickable element (checkbox_no_label),
  // not on the container row, so no FocusClusterRoot/InFocusCluster needed.
  if (checkbox_ent) {
    changed = true;
  }

  return {changed, entity, value};
}

template <size_t Size>
ElementResult
checkbox_group(HasUIContext auto &ctx, EntityParent ep_pair,
               std::bitset<Size> &values,
               const std::array<std::string_view, Size> &labels = {{}},
               std::pair<int, int> min_max = {-1, -1},
               ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto max_height = config.size.y_axis;
  config.size.y_axis = children();
  _init_component(ctx, ep_pair, config, ComponentType::CheckboxGroup, false,
                  "checkbox_group");
  config.size.y_axis = max_height;

  int count = (int)values.count();

  const auto should_disable = [min_max, count](bool value) -> bool {
    // we should disable, if not checked and we are at the cap
    bool at_cap = !value && min_max.second != -1 && count >= min_max.second;
    // we should disable, if checked and we are at the min
    bool at_min = value && min_max.first != -1 && count <= min_max.first;
    return at_cap || at_min;
  };

  bool changed = false;
  for (size_t i = 0; i < values.size(); ++i) {
    bool value = values.test(i);

    if (checkbox(
            ctx, mk(entity, i), value,
            ComponentConfig::inherit_from(config,
                                          fmt::format("checkbox row {}", i))
                .with_size(config.size)
                .with_label(i < labels.size() ? std::string(labels[i]) : "")
                .with_color_usage(Theme::Usage::None)
                .with_flex_direction(FlexDirection::Row)
                .with_disabled(should_disable(value))
                .with_render_layer(config.render_layer))) {
      changed = true;
      if (value)
        values.set(i);
      else
        values.reset(i);
    }
  }

  return {changed, entity, values};
}

enum struct ToggleSwitchStyle {
  Pill,   // iOS-style pill with sliding knob (default)
  Circle, // Single circle with X/checkmark inside
};

/// Toggle switch with style variants: Pill (iOS-style) or Circle (X/checkmark).
ElementResult
toggle_switch(HasUIContext auto &ctx, EntityParent ep_pair, bool &value,
              ComponentConfig config = ComponentConfig(),
              ToggleSwitchStyle style = ToggleSwitchStyle::Pill) {
  auto [entity, parent] = deref(ep_pair);

  auto label = config.label;
  config.label = "";
  _init_component(ctx, ep_pair, config, ComponentType::ToggleSwitch, false,
                  "toggle_switch_row");

  HasToggleSwitchState &state =
      _init_state<HasToggleSwitchState>(entity, [&](auto &) {}, value);

  // Animate (smooth lerp toward target)
  float target = state.on ? 1.0f : 0.0f;
  state.animation_progress += (target - state.animation_progress) * 0.2f;

  const Theme &theme = ctx.theme;

  // Optional label
  if (!label.empty()) {
    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "toggle_label")
            .with_size(config.size._scale_x(0.7f))
            .with_label(label)
            .with_color_usage(Theme::Usage::None));
  }

  bool clicked = false;

  if (style == ToggleSwitchStyle::Circle) {
    // Clean radio-button: filled circle when ON, empty ring when OFF
    float sz = 18.0f;
    Color bg = state.on ? theme.accent : theme.background;
    Color border = state.on ? theme.accent : theme.font_muted;

    auto circ = button(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "toggle_circle")
            .with_size(ComponentSize{pixels((int)sz), pixels((int)sz)})
            .with_custom_background(bg)
            .with_border(border, 2.0f)
            .with_rounded_corners(RoundedCorners().all_round())
            .with_roundness(1.0f));

    clicked = circ;

    // Checkmark when ON
    if (state.on) {
      div(ctx, mk(circ.ent()),
          ComponentConfig::inherit_from(config, "toggle_check")
              .with_label("✓")
              .with_size(ComponentSize{pixels((int)sz - 4), pixels((int)sz - 4)})
              .with_absolute_position()
              .with_translate(2.0f, 0.0f)
              .with_custom_text_color(theme.background)
              .with_font(UIComponent::DEFAULT_FONT, 12.0f)
              .with_alignment(TextAlignment::Center)
              .with_skip_tabbing(true));
    }
  } else {
    // Pill style (default)
    Color track_color = colors::lerp(theme.secondary, theme.accent, state.animation_progress);
    // Knob color indicates state - accent when ON, light grey when OFF
    // This ensures state is visible even when track is overridden by hover
    Color knob_off = theme.accent;
    Color knob_on = theme.accent;
    Color knob_color = colors::lerp(knob_off, knob_on, state.animation_progress);
    float track_w = 40.0f, track_h = 20.0f, knob_sz = 16.0f, pad = 2.0f;

    auto track_result = button(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "toggle_track")
            .with_size(ComponentSize{pixels((int)track_w), pixels((int)track_h)})
            .with_custom_background(track_color)
            .with_rounded_corners(RoundedCorners().all_round())
            .with_roundness(0.5f));

    clicked = track_result;

    // Sliding knob
    float knob_x = pad + (track_w - knob_sz - pad * 2) * state.animation_progress;
    div(ctx, mk(track_result.ent()),
        ComponentConfig::inherit_from(config, "toggle_knob")
            .with_size(ComponentSize{pixels((int)knob_sz), pixels((int)knob_sz)})
            .with_absolute_position()
            .with_translate(knob_x, pad)
            .with_custom_background(knob_color)
            .with_rounded_corners(RoundedCorners().all_round())
            .with_roundness(1.0f)
            .with_skip_tabbing(true));
  }

  if (clicked) {
    state.on = !state.on;
    state.changed_since = true;
  }

  value = state.on;
  ElementResult result{state.changed_since, entity, value};
  state.changed_since = false;
  return result;
}

// Helper function to generate label text based on position and value
static std::string
generate_label_text(const std::string &original_label, float value,
                    SliderHandleValueLabelPosition position) {
  switch (position) {
  case SliderHandleValueLabelPosition::None:
  case SliderHandleValueLabelPosition::OnHandle:
    return original_label;
  case SliderHandleValueLabelPosition::WithLabel:
    return original_label + ": " +
           std::to_string(static_cast<int>(value * 100)) + "%";
  case SliderHandleValueLabelPosition::WithLabelNewLine:
    return original_label + "\n" +
           std::to_string(static_cast<int>(value * 100)) + "%";
  }
  return original_label;
}

// Helper function to update label entity
static void update_label_entity(Entity &entity, const std::string &new_text) {
  if (entity.has<ui::HasLabel>()) {
    entity.get<ui::HasLabel>().set_label(new_text);
  }
}

// Helper function to find and update handle label
static void update_handle_label(Entity &handle_entity, float value) {
  UIComponent &handle_cmp = handle_entity.get<UIComponent>();
  for (EntityID child_id : handle_cmp.children) {
    Entity &child_entity = EntityHelper::getEntityForIDEnforce(child_id);
    if (child_entity.has<ui::HasLabel>()) {
      update_label_entity(child_entity,
                          std::to_string(static_cast<int>(value * 100)));
      break;
    }
  }
}

// Helper function to find and update main label
static void update_main_label(Entity &slider_entity,
                              const std::string &original_label, float value,
                              SliderHandleValueLabelPosition position) {
  UIComponent &slider_cmp = slider_entity.get<UIComponent>();
  if (!slider_cmp.children.empty()) {
    EntityID main_label_id = slider_cmp.children[0];
    Entity &main_label_entity =
        EntityHelper::getEntityForIDEnforce(main_label_id);
    std::string new_text = generate_label_text(original_label, value, position);
    update_label_entity(main_label_entity, new_text);
  }
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value,
                     ComponentConfig config = ComponentConfig(),
                     SliderHandleValueLabelPosition handle_label_position =
                         SliderHandleValueLabelPosition::None) {
  auto [entity, parent] = deref(ep_pair);

  std::string original_label = config.label;
  config.label = "";

  // Compact mode: skip label area when no label provided
  bool compact = original_label.empty();

  auto original_color_usage = config.color_usage;
  config.with_color_usage(Theme::Usage::None);
  _init_component(ctx, ep_pair, config, ComponentType::Slider, true, "slider");
  config.color_usage = original_color_usage;

  // Create main label (only when label is provided)
  if (!compact) {
    std::string main_label_text =
        generate_label_text(original_label, owned_value, handle_label_position);
    auto label_corners = RoundedCorners(config.rounded_corners.value())
                             .sharp(TOP_RIGHT)
                             .sharp(BOTTOM_RIGHT);

    auto label = div(ctx, mk(entity, entity.id + 0),
                     ComponentConfig::inherit_from(config, "slider_text")
                         .with_size(config.size)
                         .with_label(main_label_text)
                         .with_color_usage(Theme::Usage::Primary)
                         .with_rounded_corners(label_corners)
                         .with_render_layer(config.render_layer + 0));
    label.ent()
        .template get<UIComponent>()
        .set_desired_width(config.size._scale_x(0.5f).x_axis)
        .set_desired_height(config.size.y_axis);
    label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // Create slider background
  // In compact mode, use full rounded corners; otherwise sharp on left
  auto elem_corners = compact
      ? RoundedCorners(config.rounded_corners.value())
      : RoundedCorners(config.rounded_corners.value()).left_sharp();
  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0),
                  ComponentConfig::inherit_from(config, "slider_background")
                      .with_size(config.size)
                      .with_color_usage(Theme::Usage::Secondary)
                      .with_rounded_corners(elem_corners)
                      .with_render_layer(config.render_layer + 1));

  elem.ent().template get<UIComponent>().set_desired_width(config.size.x_axis);

  Entity &slider_bg = elem.ent();
  slider_bg.template addComponentIfMissing<InFocusCluster>();

  if (slider_bg.is_missing<ui::HasSliderState>())
    slider_bg.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = slider_bg.get<ui::HasSliderState>();
  sliderState.changed_since = true;

  // Create value update function
  auto apply_slider_value = [&](Entity &target, float new_value_pct) {
    float clamped = std::clamp(new_value_pct, 0.f, 1.f);
    if (clamped == sliderState.value)
      return;
    sliderState.value = clamped;
    sliderState.changed_since = true;

    UIComponent &cmp = target.get<UIComponent>();
    Rectangle rect = cmp.rect();
    if (!cmp.children.empty()) {
      EntityID child_id = cmp.children[0];
      Entity &child = EntityHelper::getEntityForIDEnforce(child_id);
      UIComponent &child_cmp = child.get<UIComponent>();
      child_cmp.set_desired_padding(
          pixels(sliderState.value * 0.75f * rect.width), Axis::left);

      // Update labels based on position
      if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
        update_handle_label(child, sliderState.value);
      } else if (handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabel ||
                 handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabelNewLine) {
        update_main_label(target, original_label, sliderState.value,
                          handle_label_position);
      }
    }
  };

  // Add drag listener
  slider_bg.addComponentIfMissing<ui::HasDragListener>(
      [apply_slider_value](Entity &draggable) {
        UIComponent &cmp = draggable.get<UIComponent>();
        Rectangle rect = cmp.rect();
        auto mouse_position = input::get_mouse_position();
        float v = (mouse_position.x - rect.x) / rect.width;
        apply_slider_value(draggable, v);
      });

  // Create handle
  const auto dim = config.size.x_axis.dim;
  const float width_val = config.size.x_axis.value;

  // Warn about tiny widths
  const bool tiny_width =
      (dim == Dim::Pixels && width_val < 8.0f) ||
      ((dim == Dim::Percent || dim == Dim::ScreenPercent) && width_val < 0.02f);
  if (tiny_width) {
    log_warn("slider width is very small (dim={}, value={:.4f}); slider handle "
             "may be invisible (component: {})",
             (int)dim, width_val, config.debug_name.c_str());
  }

  Size handle_width_size{dim, width_val * 0.25f, config.size.x_axis.strictness};
  if (dim == Dim::Pixels)
    handle_width_size.value = std::max(2.0f, handle_width_size.value);
  else if (dim == Dim::Percent || dim == Dim::ScreenPercent)
    handle_width_size.value = std::max(0.02f, handle_width_size.value);

  Size handle_left_size{dim, owned_value * 0.75f * width_val,
                        config.size.x_axis.strictness};
  if (dim == Dim::Pixels)
    handle_left_size.value = std::max(0.0f, handle_left_size.value);
  else if (dim == Dim::Percent || dim == Dim::ScreenPercent)
    handle_left_size.value = std::max(0.0f, handle_left_size.value);

  auto handle_config =
      ComponentConfig::inherit_from(config, "slider_handle")
          .with_size(ComponentSize{handle_width_size, config.size.y_axis})
          .with_padding(Padding{.left = handle_left_size})
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(config.rounded_corners.value())
          .with_debug_name("slider_handle")
          .with_render_layer(config.render_layer + 2);

  auto handle = div(ctx, mk(slider_bg), handle_config);
  handle.cmp()
      .set_desired_width(handle_config.size.x_axis)
      .set_desired_height(config.size.y_axis);
  handle.ent().template addComponentIfMissing<InFocusCluster>();

  // Add handle label if needed
  if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
    std::string handle_label_text =
        std::to_string(static_cast<int>(sliderState.value * 100));
    auto handle_label_config =
        ComponentConfig::inherit_from(config, "slider_handle_label")
            .with_label(handle_label_text)
            .with_size(ComponentSize{handle_width_size, config.size.y_axis})
            .with_color_usage(Theme::Usage::Primary)
            .with_render_layer(config.render_layer + 3)
            .with_font(config.font_name, config.font_size);

    auto handle_label = div(ctx, mk(handle.ent()), handle_label_config);
    handle_label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // Add keyboard listener
  slider_bg.addComponentIfMissing<ui::HasLeftRightListener>(
      [apply_slider_value, &sliderState](Entity &ent, int dir) {
        const float step = 0.01f;
        apply_slider_value(ent, sliderState.value + (dir < 0 ? -step : step));
      });

  owned_value = sliderState.value;
  entity.template addComponentIfMissing<FocusClusterRoot>();
  return ElementResult{sliderState.changed_since, entity, sliderState.value};
}

template <typename Container>
ElementResult pagination(HasUIContext auto &ctx, EntityParent ep_pair,
                         const Container &options, size_t &option_index,
                         ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  if (entity.is_missing<ui::HasDropdownState>())
    entity.addComponent<ui::HasDropdownState>(
        options, nullptr, [&](size_t opt) {
          HasDropdownState &ds = entity.get<ui::HasDropdownState>();
          if (!ds.on) {
            ds.last_option_clicked = opt;
          }
        });
  HasDropdownState &dropdownState = entity.get<ui::HasDropdownState>();
  dropdownState.last_option_clicked = (size_t)option_index;
  dropdownState.changed_since = false;

  const auto on_option_click = [options, &ctx](Entity &dd, size_t i) {
    size_t index = i % options.size();
    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.last_option_clicked = index;
    ds.on = !ds.on;
    ds.changed_since = true;

    EntityID id = dd.get<UIComponent>().children[i];
    ctx.set_focus(id);
  };

  // Use styling defaults for size if none provided
  config.flex_direction = FlexDirection::Row;

  std::string label_str = config.label;
  config.label = "";

  bool first_time = _init_component(
      ctx, ep_pair, config, ComponentType::Pagination, false, "pagination");

  int child_index = 0;

  if (button(
          ctx, mk(entity),
          ComponentConfig::inherit_from(config, "left")
              .with_size(ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis})
              .with_label("<")
              .with_font(UIComponent::SYMBOL_FONT, 16.f)
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, prev_index(option_index - 1, options.size()));
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(
            ctx, mk(entity, child_index + i),
            ComponentConfig::inherit_from(config,
                                          fmt::format("option {}", i + 1))
                .with_size(ComponentSize{pixels(default_component_size.x / 2.f),
                                         config.size.y_axis})
                .with_label(std::string(options[i]))
                .with_render_layer(config.render_layer + 1))) {
      on_option_click(entity, i + 1);
    }
  }

  if (button(
          ctx, mk(entity),
          ComponentConfig::inherit_from(config, "right")
              .with_size(ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis})
              .with_label(">")
              .with_font(UIComponent::SYMBOL_FONT, 16.f)
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, next_index(option_index, options.size()));
  }

  if (first_time) {
    EntityID id = entity.get<UIComponent>()
                      .children[dropdownState.last_option_clicked + 1];
    ctx.set_focus(id);
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

template <typename Container>
ElementResult dropdown(HasUIContext auto &ctx, EntityParent ep_pair,
                       const Container &options, size_t &option_index,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasDropdownState &dropdownState = _init_state<HasDropdownState>(
      entity,
      [&](auto &hdds) {
        hdds.last_option_clicked = option_index;
        hdds.changed_since = false;
      },
      options, nullptr,
      [&](size_t opt) {
        HasDropdownState &ds = entity.get<ui::HasDropdownState>();
        if (!ds.on) {
          ds.last_option_clicked = opt;
        }
      });

  if (config.size.is_default) {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def =
            styling_defaults.get_component_config(ComponentType::Dropdown)) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(children(default_component_size.x),
                                  pixels(default_component_size.y));
    }
  }

  std::string label_str = config.label;
  config.label = "";
  config.flex_direction = FlexDirection::Row;

  _init_component(ctx, ep_pair, config, ComponentType::Dropdown);

  auto button_corners =
      config.rounded_corners.value_or(ctx.theme.rounded_corners);

  auto config_size = config.size;

  bool has_label_child = !label_str.empty();
  if (has_label_child) {
    config_size = config.size._scale_x(0.5f);
    button_corners = RoundedCorners(button_corners).left_sharp();

    auto label = div(
        ctx, mk(entity),
        ComponentConfig::inherit_from(config, "dropdown_label")
            .with_size(config_size)
            .with_label(std::string(label_str))
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(RoundedCorners(button_corners).right_sharp())
            .with_render_layer(config.render_layer + 0));
    label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  const auto toggle_visibility = [&](Entity &) {
    dropdownState.on = !dropdownState.on;
  };

  const auto on_option_click = [&](Entity &dd, size_t option_index) {
    toggle_visibility(dd);
    dropdownState.last_option_clicked = option_index;
    dropdownState.changed_since = true;

    EntityID id = entity.get<UIComponent>().children[label_str.empty() ? 0 : 1];
    Entity &first_child = EntityHelper::getEntityForIDEnforce(id);

    first_child.get<ui::HasLabel>().label = (options[option_index]);
    ctx.set_focus(first_child.id);
  };

  auto current_option = std::string(
      options[dropdownState.on ? 0 : dropdownState.last_option_clicked]);
  auto drop_arrow_icon = dropdownState.on ? " ^" : " V";
  auto main_button_label = fmt::format("{}{}", current_option, drop_arrow_icon);
  // TODO hot sibling summary: previously, when a label was present to the
  // left of the dropdown button, we passed that label entity id as a "hot
  // sibling" to the main button so hovering/focusing the button would
  // visually hot the label too. Implementation details we removed:
  // - ComponentConfig had a std::vector<EntityID> hot_siblings with builder
  //   helpers with_hot_siblings/add_hot_sibling.
  // - Applying config added a ui::BringsHotSiblings component to the target
  //   entity, storing those ids.
  // - In rendering, when an entity became hot, we iterated its parent's
  //   children and, for each sibling entity that had BringsHotSiblings
  //   including the current entity id, we treated that sibling as hot as
  //   well.
  // - In this dropdown, when a label existed, we collected the label child id
  //   and passed it via with_hot_siblings({label_id}) to the main button.
  // Re-adding this would require restoring: ComponentConfig hot_siblings api,
  // ui::BringsHotSiblings component, and the rendering propagation logic.
  auto main_btn = button(
      ctx, mk(entity),
      ComponentConfig::inherit_from(config, "option 1")
          .with_size(config_size)
          .with_label(main_button_label)
          .with_rounded_corners(button_corners)
          // TODO This works great but we need a way to
          // close the dropdown when you leave without selecting anything
          //  .with_select_on_focus(true)
          .with_render_layer(config.render_layer + (dropdownState.on ? 0 : 0)));
  if (main_btn) {
    dropdownState.on ? on_option_click(entity, 0) : toggle_visibility(entity);
  }

  // Mark the label + main dropdown button as a focus cluster, but do not
  // include dropdown items (they should be separately focusable when open).
  entity.template addComponentIfMissing<FocusClusterRoot>();
  // Mark the main dropdown button as part of the cluster
  main_btn.ent().template addComponentIfMissing<InFocusCluster>();

  if (auto result = button_group(
          ctx, mk(entity), options,
          ComponentConfig::inherit_from(config, "dropdown button group")
              .with_hidden(config.hidden || !dropdownState.on)
              .with_render_layer(config.render_layer + 1));
      result) {
    on_option_click(entity, result.template as<int>());
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

template <typename Container>
ElementResult navigation_bar(HasUIContext auto &ctx, EntityParent ep_pair,
                             const Container &options, size_t &option_index,
                             ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasNavigationBarState &navState = _init_state<HasNavigationBarState>(
      entity,
      [&](auto &hnbs) {
        hnbs.set_current_index(option_index);
        hnbs.changed_since = false;
      },
      options, nullptr);

  if (config.size.is_default) {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def = styling_defaults.get_component_config(
            ComponentType::NavigationBar)) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(pixels(default_component_size.x),
                                  pixels(default_component_size.y));
    }
  }
  // TODO - add default
  config.flex_direction = FlexDirection::Row;

  // Prevent the parent navigation bar from getting a background color
  config.with_color_usage(Theme::Usage::None);
  _init_component(ctx, ep_pair, config, ComponentType::NavigationBar, false,
                  "navigation_bar");

  bool clicked = false;
  size_t new_index = navState.current_index();

  constexpr float arrow_ratio = 0.20f;
  constexpr float label_ratio = 1.f - (arrow_ratio * 2.f); // 60 % for label

  auto arrow_size = ComponentSize{percent(arrow_ratio), config.size.y_axis};

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "left_arrow")
                 .with_size(arrow_size)
                 .with_label("<")
                 .with_font(UIComponent::SYMBOL_FONT, 16.f)
                 .with_rounded_corners(RoundedCorners().left_round()))) {
    clicked = true;
    new_index = prev_index(navState.current_index(), options.size());
  }

  div(ctx, mk(entity),
      ComponentConfig::inherit_from(config, "center_label")
          .with_size(ComponentSize{percent(label_ratio), config.size.y_axis})
          .with_label(std::string(options[navState.current_index()]))
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(RoundedCorners().all_sharp())
          .with_skip_tabbing(true));

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "right_arrow")
                 .with_size(arrow_size)
                 .with_label(">")
                 .with_font(UIComponent::SYMBOL_FONT, 16.f)
                 .with_rounded_corners(RoundedCorners().right_round()))) {
    clicked = true;
    new_index = next_index(navState.current_index(), options.size());
  }

  if (clicked) {
    navState.set_current_index(new_index);
    navState.changed_since = true;
    if (navState.on_option_changed) {
      navState.on_option_changed(new_index);
    }
  }

  option_index = navState.current_index();
  return ElementResult{navState.changed_since, entity,
                       navState.current_index()};
}

// Progress bar display options
enum class ProgressBarLabelStyle {
  None,       // No label
  Percentage, // Show "75%"
  Fraction,   // Show "75/100"
  Custom      // Use config.label as-is
};

// Progress bar - displays a value from 0.0 to 1.0 (or custom range)
// Unlike slider, this is read-only (no interaction)
ElementResult progress_bar(
    HasUIContext auto &ctx, EntityParent ep_pair, float value,
    ComponentConfig config = ComponentConfig(),
    ProgressBarLabelStyle label_style = ProgressBarLabelStyle::Percentage,
    float min_value = 0.f, float max_value = 1.f) {
  auto [entity, parent] = deref(ep_pair);

  std::string original_label = config.label;
  config.label = "";

  // Initialize as a non-interactive div
  _init_component(ctx, ep_pair, config, ComponentType::Div, false,
                  "progress_bar");

  // Normalize value to 0-1 range
  float normalized =
      (max_value > min_value)
          ? std::clamp((value - min_value) / (max_value - min_value), 0.f, 1.f)
          : 0.f;

  // Generate label text
  std::string label_text;
  switch (label_style) {
  case ProgressBarLabelStyle::Percentage:
    label_text = fmt::format("{}%", static_cast<int>(normalized * 100));
    break;
  case ProgressBarLabelStyle::Fraction:
    label_text = fmt::format("{}/{}", static_cast<int>(value),
                             static_cast<int>(max_value));
    break;
  case ProgressBarLabelStyle::Custom:
    label_text = original_label;
    break;
  case ProgressBarLabelStyle::None:
  default:
    break;
  }

  // If there's an original label, prepend it
  if (!original_label.empty() && label_style != ProgressBarLabelStyle::Custom &&
      label_style != ProgressBarLabelStyle::None) {
    label_text = original_label + ": " + label_text;
  }

  // Create background track
  auto track_corners = config.rounded_corners.value_or(RoundedCorners().get());
  auto track = div(ctx, mk(entity, 0),
                   ComponentConfig::inherit_from(config, "progress_track")
                       .with_size(config.size)
                       .with_color_usage(Theme::Usage::Secondary)
                       .with_rounded_corners(RoundedCorners(track_corners))
                       .with_skip_tabbing(true)
                       .with_render_layer(config.render_layer));

  // Create fill bar (width based on normalized value)
  const auto x_axis = config.size.x_axis;
  Size fill_width{x_axis.dim, x_axis.value * normalized, x_axis.strictness};

  // Only render fill if there's something to show
  if (normalized > 0.001f) {
    auto fill_corners = RoundedCorners(track_corners);
    // If not fully filled, make right side sharp for clean edge
    if (normalized < 0.99f) {
      fill_corners.sharp(TOP_RIGHT).sharp(BOTTOM_RIGHT);
    }

    div(ctx, mk(track.ent(), 0),
        ComponentConfig::inherit_from(config, "progress_fill")
            .with_size(ComponentSize{fill_width, config.size.y_axis})
            .with_absolute_position()
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(fill_corners)
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer + 1));
  }

  // Add label on top if specified
  if (!label_text.empty()) {
    div(ctx, mk(track.ent(), 1),
        ComponentConfig::inherit_from(config, "progress_label")
            .with_size(config.size)
            .with_label(label_text)
            .with_absolute_position()
            .with_color_usage(Theme::Usage::None)
            .with_auto_text_color(true)
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer + 2));
  }

  return ElementResult{false, entity, normalized};
}

// Circular/radial progress indicator - displays a value from 0.0 to 1.0 as an
// arc Unlike progress_bar, this renders as a circular ring that fills clockwise
//
// Usage:
// ```cpp
// circular_progress(ctx, mk(parent),
//     0.75f,  // value 0-1
//     ComponentConfig{}
//         .with_size(pixels(80), pixels(80))
//         .with_custom_background(fill_color)
//         .with_border(track_color, 8.0f));  // border thickness = ring
//         thickness
// ```
ElementResult circular_progress(HasUIContext auto &ctx, EntityParent ep_pair,
                                float value,
                                ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Default to square size if not specified
  // Use styling defaults if available, otherwise scale to 720p baseline (50px)
  if (config.size.is_default) {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def = styling_defaults.get_component_config(
            ComponentType::CircularProgress)) {
      config.size = def->size;
    } else {
      // Square aspect ratio, scales with resolution
      Size ring_size = h720(50.0f);
      config.with_size(ComponentSize{ring_size, ring_size});
    }
  }

  // Initialize component
  _init_component(ctx, ep_pair, config, ComponentType::CircularProgress, false,
                  "circular_progress");

  // Clamp value
  float normalized = std::clamp(value, 0.0f, 1.0f);

  // Determine colors from config
  // Background color (from with_custom_background) = fill color
  // Border color (from with_border) = track color
  Color fill_color = colors::UI_GREEN;
  Color track_color = Color{128, 128, 128, 100};
  float thickness = 8.0f;

  if (config.color_usage == Theme::Usage::Custom &&
      config.custom_color.has_value()) {
    fill_color = config.custom_color.value();
  } else if (Theme::is_valid(config.color_usage)) {
    fill_color = ctx.theme.from_usage(config.color_usage);
  }

  if (config.border_config.has_value()) {
    track_color = config.border_config->color;
    thickness = config.border_config->thickness;
  }

  // Store state on entity for rendering
  auto &state = entity.template addComponentIfMissing<HasCircularProgressState>(
      normalized, thickness);
  state.set_value(normalized)
      .set_thickness(thickness)
      .set_fill_color(fill_color)
      .set_track_color(track_color);

  // Remove HasColor so the regular rectangle rendering doesn't draw a
  // background
  entity.removeComponentIfExists<HasColor>();
  // Also remove border so it doesn't render a rectangle border
  entity.removeComponentIfExists<HasBorder>();

  return ElementResult{false, entity, normalized};
}

/// Creates a single-line text input field.
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param text Reference to the string that will be edited
/// @param config Component configuration
///
/// Features:
/// - Click to focus, keyboard input when focused
/// - Backspace to delete, Enter to submit
/// - Left/Right arrows to move cursor
/// - Home/End to jump to start/end
/// - Visual cursor that blinks when focused
/// - Full UTF-8/CJK support
///
/// Usage:
/// ```cpp
/// std::string username;
/// if (text_input(ctx, mk(parent), username,
///                ComponentConfig{}.with_label("Username"))) {
///   // Text was changed
/// }
/// ```
ElementResult text_input(HasUIContext auto &ctx, EntityParent ep_pair,
                         std::string &text,
                         ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  using namespace text_input_utils;
  using InputAction =
      typename std::remove_reference_t<decltype(ctx)>::value_type;

  // Initialize state
  auto &state = _init_state<HasTextInputState>(
      entity,
      [&](HasTextInputState &s) {
        if (s.text() != text) {
          s.storage.clear();
          s.storage.insert(0, text);
          s.cursor_position = text.size();
        }
        s.changed_since = false;
      },
      text);

  // Extract label before clearing config
  std::string label = config.label;
  config.label = "";
  bool has_label = !label.empty();

  // Apply default size
  if (config.size.is_default) {
    auto def =
        UIStylingDefaults::get().get_component_config(ComponentType::TextInput);
    config.size = def ? def->size
                      : ComponentSize(pixels(default_component_size.x * 1.5f),
                                      pixels(default_component_size.y));
  }

  config.flex_direction = FlexDirection::Row;
  _init_component(ctx, ep_pair, config, ComponentType::TextInput, false,
                  "text_input");

  auto base_corners = RoundedCorners(
      config.rounded_corners.value_or(ctx.theme.rounded_corners));
  auto field_size = has_label ? config.size._scale_x(0.5f) : config.size;

  // Create label
  if (has_label) {
    div(ctx, mk(entity, 0),
        ComponentConfig::inherit_from(config, "text_input_label")
            .with_size(field_size)
            .with_label(label)
            .with_background(Theme::Usage::Primary)
            .with_rounded_corners(base_corners.right_sharp())
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer))
        .ent()
        .template addComponentIfMissing<InFocusCluster>();
  }

  // Build display text (apply mask if configured)
  std::string display_text = state.text();
  size_t display_cursor_pos = state.cursor_position;

  if (config.mask_char) {
    // Count UTF-8 codepoints for mask and cursor position
    size_t codepoint_count = 0;
    size_t codepoints_before_cursor = 0;
    for (size_t i = 0; i < display_text.size();) {
      if (i < state.cursor_position)
        codepoints_before_cursor++;
      i += text_input_utils::utf8_char_length(display_text, i);
      codepoint_count++;
    }
    display_text = std::string(codepoint_count, *config.mask_char);
    display_cursor_pos = codepoints_before_cursor;
  }

  // Create input field container
  auto field_result =
      div(ctx, mk(entity, has_label ? 1 : 0),
          ComponentConfig::inherit_from(config, "text_input_field")
              .with_size(field_size)
              .with_background(Theme::Usage::Secondary)
              .with_rounded_corners(has_label ? base_corners.left_sharp()
                                              : base_corners)
              .with_alignment(TextAlignment::Left)
              .with_padding(Padding{.top = pixels(5),
                                    .bottom = pixels(5),
                                    .left = pixels(10),
                                    .right = pixels(10)})
              .with_render_layer(config.render_layer + 1));

  auto &field_entity = field_result.ent();

  // Ensure HasLabel exists and set display text
  field_entity.template addComponentIfMissing<HasLabel>().label = display_text;

  // Update focus state - check if this field OR the parent container has focus
  field_entity.template addComponentIfMissing<InFocusCluster>();
  bool field_has_focus = ctx.has_focus(field_entity.id);
  bool parent_has_focus = ctx.has_focus(entity.id);
  state.is_focused = field_has_focus || parent_has_focus;

  // Render cursor as overlay when focused
  if (state.is_focused) {
    bool show_cursor = update_blink(state, 0.016f);

    // Calculate cursor position using the SAME font size as text rendering
    // Text rendering uses position_text_ex which may auto-size the font to fit
    auto &field_cmp = field_entity.template get<UIComponent>();
    auto font_manager = EntityHelper::get_singleton_cmp<FontManager>();

    float cursor_x = 5.f; // Default: text margin from position_text_ex
    float cursor_height = std::max(config.font_size * 0.9f, 16.f);
    float actual_font_size = config.font_size;

    if (font_manager) {
      // Get the actual rendered font size by calling position_text_ex on the
      // full text This accounts for auto-sizing when text doesn't fit
      constexpr Vector2Type TEXT_MARGIN{5.f, 5.f};
      TextPositionResult full_text_result = position_text_ex(
          *font_manager, display_text.empty() ? " " : display_text,
          field_cmp.rect(), TextAlignment::Left, TEXT_MARGIN);
      actual_font_size = full_text_result.rect.height;
      cursor_height = std::max(actual_font_size * 0.9f, 16.f);

      // Now measure the text BEFORE cursor using the actual rendered font size
      std::string font_name = config.font_name == UIComponent::UNSET_FONT
                                  ? UIComponent::DEFAULT_FONT
                                  : config.font_name;
      Font font = font_manager->get_font(font_name);

      if (!display_text.empty() && display_cursor_pos > 0) {
        size_t safe_pos = std::min(display_cursor_pos, display_text.size());
        std::string text_before = display_text.substr(0, safe_pos);
        Vector2Type text_size =
            measure_text(font, text_before.c_str(), actual_font_size, 1.f);
        cursor_x = TEXT_MARGIN.x + text_size.x;
      } else {
        cursor_x = TEXT_MARGIN.x;
      }
    }

    // Center cursor vertically in field
    float field_height = field_cmp.computed[Axis::Y];
    float cursor_y = (field_height - cursor_height) / 2.f;

    // Cursor is a thin vertical bar
    // Note: Width must be >= 8px to survive 8pt grid snapping at high DPI
    // (grid unit scales with screen, e.g. ~11px at 1080p, so 2-4px rounds to 0)
    auto cursor_result =
        div(ctx, mk(field_entity, 0),
            ComponentConfig()
                .with_size(ComponentSize(pixels(8), pixels(cursor_height)))
                .with_custom_background(ctx.theme.font) // Use theme text color
                .with_translate(cursor_x,
                                cursor_y) // Position cursor aligned with text
                .with_opacity(show_cursor ? 1.0f : 0.0f) // Blink
                .with_skip_tabbing(true)
                .with_debug_name("cursor")
                .with_render_layer(config.render_layer + 10));
  }

  // Click to focus
  field_entity.template addComponentIfMissing<HasClickListener>(
      [&ctx](Entity &ent) {
        ctx.set_focus(ent.id);
        if (ent.has<HasTextInputState>())
          reset_blink(ent.get<HasTextInputState>());
      });

  // TODO: Implement horizontal scrolling when text exceeds field width

  // Handle input when focused
  if (state.is_focused) {
    // Character input
    for (int key = input::get_char_pressed(); key > 0;
         key = input::get_char_pressed()) {
      if (insert_char(state, key))
        reset_blink(state);
    }

    // Key actions
    if (ctx.pressed(InputAction::TextBackspace) && delete_before_cursor(state))
      reset_blink(state);
    if (ctx.pressed(InputAction::TextDelete) && delete_at_cursor(state))
      reset_blink(state);
    if (ctx.pressed(InputAction::TextHome)) {
      state.cursor_position = 0;
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::TextEnd)) {
      state.cursor_position = state.text_size();
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetLeft)) {
      move_cursor_left(state);
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetRight)) {
      move_cursor_right(state);
      reset_blink(state);
    }
    if (ctx.pressed(InputAction::WidgetPress) &&
        entity.has<HasTextInputListener>())
      if (auto &listener = entity.get<HasTextInputListener>();
          listener.on_submit)
        listener.on_submit(entity);
  }

  text = state.text();
  entity.template addComponentIfMissing<FocusClusterRoot>();
  return ElementResult{state.changed_since, entity};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
