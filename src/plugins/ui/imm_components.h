#pragma once

// TODO: Move internal helper functions to a detail:: namespace to clearly
// separate public API from implementation details. Functions like prev_index,
// next_index, and other internal utilities should not be part of the public
// API. See e2e_testing/input_injector.h for an example of this pattern.

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
  Size separator_thickness = h720(8.0f * 0.25f); // 2px at 720p

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

  const bool size_default = config.size.is_default;
  auto max_height = config.size.y_axis;
  auto max_width = config.size.x_axis;
  if (size_default) {
    config.size.y_axis = children(max_height.value);
    config.size.x_axis = children(max_width.value);
  }

  _init_component(ctx, ep_pair, config, ComponentType::ButtonGroup, false,
                  "button_group");

  // For Row: divide width among buttons
  // For Column: use percent(1.0f) width to fill parent (avoids hardcoded 200px)
  if (config.flex_direction == FlexDirection::Row) {
    if (max_width.dim == Dim::Pixels) {
      config.size.x_axis = pixels(max_width.value / labels.size());
    } else if (max_width.dim == Dim::Percent ||
               max_width.dim == Dim::ScreenPercent) {
      config.size.x_axis =
          Size{max_width.dim, max_width.value / labels.size(),
               max_width.strictness};
    } else {
      config.size.x_axis = max_width;
    }
    config.size.y_axis = max_height;
  } else {
    config.size.x_axis = percent(1.0f);
    config.size.y_axis = size_default ? children(max_height.value) : max_height;
  }

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
    config.font_size = pixels(20.f); // Use accessible minimum size
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

  // Ensure checkbox row uses Row layout to place label and checkbox
  // side-by-side Checkboxes MUST use Row layout and NoWrap to prevent
  // label/checkbox wrapping
  config.with_flex_direction(FlexDirection::Row)
      .with_align_items(AlignItems::Center)
      .with_no_wrap();
  _init_component(ctx, ep_pair, config, ComponentType::Div, false,
                  "checkbox_row");

  // Add FocusClusterRoot to container for consistent focus ring on entire row
  entity.template addComponentIfMissing<FocusClusterRoot>();

  // 2025-08-11: ensure checkbox row uses responsive defaults so both label
  // and button scale with resolution. Previously, only the label used a
  // responsive size, causing the button to remain tiny at higher
  // DPIs/resolutions.
  // Only apply defaults if user hasn't explicitly specified a size.
  if (config.size.is_default) {
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

    div(ctx, mk(entity), label_config)
        .ent()
        .template addComponentIfMissing<InFocusCluster>();
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
  // Mark checkbox as part of focus cluster so focus ring shows on entire row
  checkbox_ent.ent().template addComponentIfMissing<InFocusCluster>();
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
  // Only prevent wrapping if caller hasn't explicitly configured wrap behavior
  // Default behavior prevents unexpected horizontal wrapping in Column layouts
  if (config.flex_wrap == FlexWrap::Wrap) {
    config.with_no_wrap();
  }
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

/// Radio button group - single-select with circular indicators
/// Note: Parent should have FlexDirection::Column for vertical layout
template <size_t N>
ElementResult radio_group(HasUIContext auto &ctx, EntityParent ep_pair,
                          const std::array<std::string_view, N> &labels,
                          size_t &selected_index,
                          ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  bool changed = false;

  // Circle dimensions - larger for better visibility
  float circle_sz = 18.0f;
  float dot_sz = 10.0f;
  float border_w = 2.0f;

  for (size_t i = 0; i < N; ++i) {
    bool is_selected = (i == selected_index);

    // Row button - transparent, for click handling
    auto row = button(ctx, mk(parent, 100 + i),
                      ComponentConfig{}
                          .with_size(config.size)
                          .with_label("")
                          .with_color_usage(Theme::Usage::None)
                          .with_flex_direction(FlexDirection::Row)
                          .with_align_items(AlignItems::Center)
                          .with_padding(Padding{.left = pixels(6)})
                          .with_debug_name(fmt::format("radio_{}", i)));

    if (row) {
      selected_index = i;
      changed = true;
    }

    // Outer circle ring
    Color ring_color = is_selected ? ctx.theme.accent : ctx.theme.font_muted;
    auto ring =
        div(ctx, mk(row.ent(), 0),
            ComponentConfig{}
                .with_size(ComponentSize{pixels(circle_sz), pixels(circle_sz)})
                .with_custom_background(ctx.theme.background)
                .with_border(ring_color, border_w)
                .with_rounded_corners(RoundedCorners().all_round())
                .with_roundness(1.0f)
                .with_margin(Margin{.right = pixels(10)})
                .with_skip_tabbing(true)
                .with_debug_name(fmt::format("radio_ring_{}", i)));

    // Inner filled dot when selected
    if (is_selected) {
      float offset = (circle_sz - dot_sz) / 2.0f;
      div(ctx, mk(ring.ent(), 0),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(dot_sz), pixels(dot_sz)})
              .with_absolute_position()
              .with_translate(offset, offset)
              .with_custom_background(ctx.theme.accent)
              .with_rounded_corners(RoundedCorners().all_round())
              .with_roundness(1.0f)
              .with_skip_tabbing(true)
              .with_debug_name(fmt::format("radio_dot_{}", i)));
    }

    // Label - positioned after circle
    auto label_ent =
        div(ctx, mk(row.ent(), 1),
            ComponentConfig{}
                .with_size(ComponentSize{pixels(150), config.size.y_axis})
                .with_label(std::string(labels[i]))
                .with_font(config.font_name, config.font_size)
                .with_custom_text_color(ctx.theme.font)
                .with_skip_tabbing(true)
                .with_debug_name(fmt::format("radio_label_{}", i)));

    // Force left alignment
    if (label_ent.ent().template has<HasLabel>()) {
      label_ent.ent().template get<HasLabel>().set_alignment(
          TextAlignment::Left);
    }
  }

  return {changed, parent, static_cast<int>(selected_index)};
}

enum struct ToggleSwitchStyle {
  Pill,   // iOS-style pill with sliding knob (default)
  Circle, // Single circle with X/checkmark inside
};

/// Toggle switch with style variants: Pill (iOS-style) or Circle (X/checkmark).
ElementResult toggle_switch(HasUIContext auto &ctx, EntityParent ep_pair,
                            bool &value,
                            ComponentConfig config = ComponentConfig(),
                            ToggleSwitchStyle style = ToggleSwitchStyle::Pill) {
  auto [entity, parent] = deref(ep_pair);

  auto label = config.label;
  config.label = "";
  // Ensure toggle row uses Row layout to place label and toggle side-by-side
  // Only set flex direction if not already specified by caller
  if (config.flex_direction == FlexDirection::Column) {
    config.with_flex_direction(FlexDirection::Row);
  }
  config.with_align_items(AlignItems::Center);
  // Only prevent wrapping if caller hasn't explicitly configured wrap behavior
  if (config.flex_wrap == FlexWrap::Wrap) {
    config.with_no_wrap();
  }
  _init_component(ctx, ep_pair, config, ComponentType::ToggleSwitch, false,
                  "toggle_switch_row");

  // Add FocusClusterRoot to container for consistent focus ring on entire row
  entity.template addComponentIfMissing<FocusClusterRoot>();

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
            .with_color_usage(Theme::Usage::None))
        .ent()
        .template addComponentIfMissing<InFocusCluster>();
  }

  bool clicked = false;

  if (style == ToggleSwitchStyle::Circle) {
    // Clean checkbox: filled circle when ON, empty ring when OFF
    Size sz = h720(18.0f);
    Size border_w = h720(2.0f);
    Color bg = state.on ? theme.accent : theme.background;
    Color border_color = state.on ? theme.accent : theme.font_muted;

    auto circ = button(ctx, mk(entity),
                       ComponentConfig::inherit_from(config, "toggle_circle")
                           .with_size(ComponentSize{sz, sz})
                           .with_custom_background(bg)
                           .with_border(border_color, border_w)
                           .with_rounded_corners(RoundedCorners().all_round())
                           .with_roundness(1.0f));

    // Mark button as part of focus cluster so focus ring shows on entire row
    circ.ent().template addComponentIfMissing<InFocusCluster>();
    clicked = circ;

    // Checkmark when ON
    if (state.on) {
      Size check_sz = h720(14.0f);
      // Offset = (18px - 14px) / 2 = 2px at 720p baseline
      Size check_offset = h720((18.0f - 14.0f) / 2.0f);
      div(ctx, mk(circ.ent()),
          ComponentConfig::inherit_from(config, "toggle_check")
              .with_label("✓")
              .with_size(ComponentSize{check_sz, check_sz})
              .with_absolute_position()
              .with_translate(check_offset, pixels(0.0f))
              .with_custom_text_color(theme.background)
              .with_font(UIComponent::DEFAULT_FONT, h720(12.0f))
              .with_alignment(TextAlignment::Center)
              .with_skip_tabbing(true));
    }
  } else {
    // Pill style (default) - responsive sizing at 720p baseline
    Color track_color =
        colors::lerp(theme.secondary, theme.accent, state.animation_progress);
    // Knob is white or uses darkfont if available for contrast
    Color knob_color =
        theme.darkfont.a > 0 ? theme.darkfont : Color{255, 255, 255, 255};
    // Responsive sizing at 720p baseline (values in 720p pixels)
    constexpr float track_w_px = 40.0f, track_h_px = 20.0f, knob_sz_px = 16.0f,
                    pad_px = 2.0f;
    Size track_w = h720(track_w_px), track_h = h720(track_h_px),
         knob_sz = h720(knob_sz_px);
    Size pad = h720(pad_px);

    auto track_result =
        button(ctx, mk(entity),
               ComponentConfig::inherit_from(config, "toggle_track")
                   .with_size(ComponentSize{track_w, track_h})
                   .with_custom_background(track_color)
                   .with_rounded_corners(RoundedCorners().all_round())
                   .with_roundness(0.5f));

    // Mark button as part of focus cluster so focus ring shows on entire row
    track_result.ent().template addComponentIfMissing<InFocusCluster>();
    clicked = track_result;

    // Sliding knob - calculate position using 720p pixel values, then wrap in
    // h720
    float knob_x_px = pad_px + (track_w_px - knob_sz_px - pad_px * 2) *
                                   state.animation_progress;
    div(ctx, mk(track_result.ent()),
        ComponentConfig::inherit_from(config, "toggle_knob")
            .with_size(ComponentSize{knob_sz, knob_sz})
            .with_absolute_position()
            .with_translate(h720(knob_x_px), pad)
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
  // TODO: slider_background can overflow by ~1-2px when parent is constrained
  // by layout. Percent sizing doesn't work because it resolves against
  // configured size, not laid-out size. Need layout system fix or tolerance.
  auto elem_corners =
      compact ? RoundedCorners(config.rounded_corners.value())
              : RoundedCorners(config.rounded_corners.value()).left_sharp();
  ComponentSize bg_size = config.size;
  if (bg_size.x_axis.dim == Dim::Pixels) {
    bg_size.x_axis.value = std::max(0.0f, bg_size.x_axis.value - 4.0f);
  } else if (bg_size.x_axis.dim == Dim::Percent ||
             bg_size.x_axis.dim == Dim::ScreenPercent) {
    bg_size = bg_size._scale_x(0.95f);
  }

  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0),
                  ComponentConfig::inherit_from(config, "slider_background")
                      .with_size(bg_size)
                      .with_color_usage(Theme::Usage::Secondary)
                      .with_rounded_corners(elem_corners)
                      .with_render_layer(config.render_layer + 1));

  elem.ent().template get<UIComponent>().set_desired_width(bg_size.x_axis);

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
  // Only prevent wrapping if caller hasn't explicitly configured wrap behavior
  if (config.flex_wrap == FlexWrap::Wrap) {
    config.with_no_wrap();
  }

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
              .with_no_wrap()
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, prev_index(option_index - 1, options.size()));
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(
            ctx, mk(entity, child_index + i),
            ComponentConfig::inherit_from(config,
                                          fmt::format("option {}", i + 1))
                .with_size(ComponentSize{pixels(default_component_size.x * 0.75f),
                                         config.size.y_axis})
                .with_label(std::string(options[i]))
                .with_no_wrap()
                .with_padding(Spacing::md)
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
              .with_no_wrap()
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
              .with_size(config.size)
              .with_flex_direction(FlexDirection::Column)
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
                 .with_rounded_corners(RoundedCorners().left_round())
                 .with_margin(Margin{}))) {
    clicked = true;
    new_index = prev_index(navState.current_index(), options.size());
  }

  div(ctx, mk(entity),
      ComponentConfig::inherit_from(config, "center_label")
          .with_size(ComponentSize{percent(label_ratio), config.size.y_axis})
          .with_label(std::string(options[navState.current_index()]))
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(RoundedCorners().all_sharp())
          .with_skip_tabbing(true)
          .with_margin(Margin{}));

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "right_arrow")
                 .with_size(arrow_size)
                 .with_label(">")
                 .with_font(UIComponent::SYMBOL_FONT, 16.f)
                 .with_rounded_corners(RoundedCorners().right_round())
                 .with_margin(Margin{}))) {
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

/// Tab container - horizontal row of tabs for organizing content into panels
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param tab_labels Container of tab label strings
/// @param active_tab Reference to the currently active tab index
/// @param config Component configuration
///
/// Features:
/// - Horizontal row of equally-sized tabs
/// - Active tab highlighting (different background)
/// - Click to switch tabs
/// - Keyboard navigation (arrows when focused)
///
/// Usage:
/// ```cpp
/// size_t current_tab = 0;
/// std::array<std::string_view, 3> tabs = {"Tab one", "Tab two", "Tab three"};
///
/// if (auto result = tab_container(ctx, mk(parent), tabs, current_tab); result)
/// {
///   // Tab changed - play sound, log, etc.
/// }
///
/// // Render content based on current_tab (updated by tab_container)
/// render_tab_content[current_tab](ctx, parent, theme);
/// ```
template <typename Container>
ElementResult tab_container(HasUIContext auto &ctx, EntityParent ep_pair,
                            const Container &tab_labels, size_t &active_tab,
                            ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (tab_labels.empty())
    return {false, entity};

  // Apply styling defaults if available
  if (config.size.is_default) {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def = styling_defaults.get_component_config(
            ComponentType::TabContainer)) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(percent(1.0f), pixels(48.f));
    }
  }

  config.flex_direction = FlexDirection::Row;
  config.with_color_usage(Theme::Usage::None);
  _init_component(ctx, ep_pair, config, ComponentType::TabContainer, false,
                  "tab_container");

  bool changed = false;
  const size_t num_tabs = tab_labels.size();

  // Calculate tab width as equal portions
  float tab_width_percent = 1.0f / static_cast<float>(num_tabs);

  size_t i = 0;
  for (const auto &label : tab_labels) {
    bool is_active = (i == active_tab);

    // Active tab: surface color with normal text
    // Inactive tab: background color with muted/dimmed text
    Color tab_bg = is_active ? ctx.theme.surface : ctx.theme.background;
    Color tab_text = is_active ? ctx.theme.font : ctx.theme.font_muted;

    auto tab_config =
        ComponentConfig::inherit_from(config, fmt::format("tab_{}", i))
            .with_size(
                ComponentSize{percent(tab_width_percent), config.size.y_axis})
            .with_label(std::string(label))
            .with_custom_background(tab_bg)
            .with_custom_text_color(tab_text)
            .with_align_items(AlignItems::Center)
            .with_justify_content(JustifyContent::Center);

    if (button(ctx, mk(entity, i), tab_config)) {
      active_tab = i;
      changed = true;
    }

    ++i;
  }

  return ElementResult{changed, entity, static_cast<int>(active_tab)};
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
    // Resolve Size to pixels for thickness
    float screen_height = 720.f;
    if (auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>()) {
      screen_height = static_cast<float>(pcr->current_resolution.height);
    }
    thickness =
        resolve_to_pixels(config.border_config->thickness, screen_height);
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

/// Frame style variants for decorative_frame()
enum struct DecorativeFrameStyle {
  KraftPaper, // Layered borders with corner accents (scrapbook feel)
  Simple,     // Single border with background
  Inset,      // Inset/sunken effect with shadow
};

/// Creates a decorative frame/border around content.
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param config Component configuration
/// @param style Frame style variant (default: KraftPaper)
///
/// Features:
/// - Multiple layered borders for depth
/// - Corner accent decorations
/// - Configurable colors via theme or custom
/// - Non-interactive (purely decorative)
///
/// Usage:
/// ```cpp
/// // Simple kraft-paper style frame using theme colors
/// auto frame = decorative_frame(ctx, mk(parent),
///     ComponentConfig{}
///         .with_size(pixels(400), pixels(300))
///         .with_background(Theme::Usage::Secondary));
///
/// // Custom colored frame
/// auto frame = decorative_frame(ctx, mk(parent),
///     ComponentConfig{}
///         .with_size(pixels(400), pixels(300))
///         .with_custom_background(kraft_tan)
///         .with_border(frame_brown, 12.0f));
/// ```
ElementResult decorative_frame(HasUIContext auto &ctx, EntityParent ep_pair,
                               ComponentConfig config = ComponentConfig(),
                               DecorativeFrameStyle style =
                                   DecorativeFrameStyle::KraftPaper) {
  auto [entity, parent] = deref(ep_pair);

  // Apply styling defaults if available
  if (config.size.is_default) {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def = styling_defaults.get_component_config(
            ComponentType::DecorativeFrame)) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(percent(1.0f), percent(1.0f));
    }
  }

  // Resolve screen height for sizing
  float screen_height = 720.f;
  if (auto *pcr = EntityHelper::get_singleton_cmp<
          window_manager::ProvidesCurrentResolution>()) {
    screen_height = static_cast<float>(pcr->current_resolution.height);
  }

  // Get frame thickness from border config or use responsive default
  Size frame_thickness_size = config.border_config.has_value()
                                  ? config.border_config->thickness
                                  : h720(12.0f);
  float frame_thickness = resolve_to_pixels(frame_thickness_size, screen_height);

  // Determine colors
  // Frame color: from border config, or derive from theme
  // Background color: from custom_color or theme
  Color frame_color = config.border_config.has_value()
                          ? config.border_config->color
                          : ctx.theme.secondary;
  Color bg_color = config.custom_color.value_or(ctx.theme.surface);

  // Initialize main container (no background - children will draw it)
  config.with_color_usage(Theme::Usage::None);
  _init_component(ctx, ep_pair, config, ComponentType::DecorativeFrame, false,
                  "decorative_frame");

  // Get computed size for positioning edge elements (corners, highlights)
  // Note: On first frame, computed values may be 0 - edge elements skip rendering
  UIComponent &cmp = entity.template get<UIComponent>();
  float w = cmp.computed[Axis::X];
  float h = cmp.computed[Axis::Y];
  bool has_computed_size = w > 0 && h > 0;

  // Inner layers fill the decorative_frame using computed pixel values
  // This avoids percentage compounding issues across nested elements
  float fill_w = w > 0 ? w : 100.f;
  float fill_h = h > 0 ? h : 100.f;
  ComponentSize fill_size{pixels(fill_w), pixels(fill_h)};

  if (style == DecorativeFrameStyle::KraftPaper) {
    // Kraft paper style: multiple layered borders with corner accents

    // Outer dark border (fills the whole frame area)
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(fill_size)
            .with_custom_background(frame_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_outer"));

    // Inner lighter border (creates depth)
    Color lighter_frame = colors::lighten(frame_color, 0.1f);

    Size inset1 = pixels(frame_thickness * 0.3f);
    div(ctx, mk(entity, 1),
        ComponentConfig{}
            .with_size(fill_size)
            .with_absolute_position()
            .with_translate(inset1, inset1)
            .with_margin(Margin{.top = inset1,
                                .bottom = inset1,
                                .left = inset1,
                                .right = inset1})
            .with_custom_background(lighter_frame)
            .with_skip_tabbing(true)
            .with_debug_name("frame_inner1"));

    // Main background
    Size inset2 = pixels(frame_thickness);
    div(ctx, mk(entity, 2),
        ComponentConfig{}
            .with_size(fill_size)
            .with_absolute_position()
            .with_translate(inset2, inset2)
            .with_margin(Margin{.top = inset2,
                                .bottom = inset2,
                                .left = inset2,
                                .right = inset2})
            .with_custom_background(bg_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_bg"));

    // Corner accents for "hand-made" feel (only render when size is computed)
    if (has_computed_size) {
      Size corner_size = h720(8.0f);
      Size corner_offset = h720(2.0f);
      float corner_size_px = resolve_to_pixels(corner_size, screen_height);
      float corner_offset_px = resolve_to_pixels(corner_offset, screen_height);
      Color corner_color = colors::darken(frame_color, 0.85f);

      // Top-left corner
      div(ctx, mk(entity, 3),
          ComponentConfig{}
              .with_size(ComponentSize{corner_size, corner_size})
              .with_absolute_position()
              .with_translate(corner_offset, corner_offset)
              .with_custom_background(corner_color)
              .with_skip_tabbing(true)
              .with_debug_name("corner_tl"));

      // Top-right corner
      div(ctx, mk(entity, 4),
          ComponentConfig{}
              .with_size(ComponentSize{corner_size, corner_size})
              .with_absolute_position()
              .with_translate(w - corner_size_px - corner_offset_px, corner_offset_px)
              .with_custom_background(corner_color)
              .with_skip_tabbing(true)
              .with_debug_name("corner_tr"));

      // Bottom-left corner
      div(ctx, mk(entity, 5),
          ComponentConfig{}
              .with_size(ComponentSize{corner_size, corner_size})
              .with_absolute_position()
              .with_translate(corner_offset_px, h - corner_size_px - corner_offset_px)
              .with_custom_background(corner_color)
              .with_skip_tabbing(true)
              .with_debug_name("corner_bl"));

      // Bottom-right corner
      div(ctx, mk(entity, 6),
          ComponentConfig{}
              .with_size(ComponentSize{corner_size, corner_size})
              .with_absolute_position()
              .with_translate(w - corner_size_px - corner_offset_px,
                              h - corner_size_px - corner_offset_px)
              .with_custom_background(corner_color)
              .with_skip_tabbing(true)
              .with_debug_name("corner_br"));
    }

  } else if (style == DecorativeFrameStyle::Simple) {
    // Simple style: just background with border
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(fill_size)
            .with_custom_background(bg_color)
            .with_border(frame_color, frame_thickness)
            .with_skip_tabbing(true)
            .with_debug_name("frame_simple"));

  } else if (style == DecorativeFrameStyle::Inset) {
    // Inset style: sunken effect with shadow
    Color shadow_color = colors::opacity_pct(colors::darken(frame_color, 0.8f), 0.6f);
    Color highlight_color = colors::lighten(frame_color, 0.2f);

    // Outer frame
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(fill_size)
            .with_custom_background(frame_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_outer"));

    // Shadow/highlight edges - only render when size is computed
    // (uses fixed pixel sizes to ensure edges stay within bounds)
    if (has_computed_size) {
      Size edge = h720(3.0f);
      float edge_px = resolve_to_pixels(edge, screen_height);

      // Shadow edge top (spans full width)
      div(ctx, mk(entity, 1),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(w), edge})
              .with_absolute_position()
              .with_translate(0.0f, 0.0f)
              .with_custom_background(shadow_color)
              .with_skip_tabbing(true)
              .with_debug_name("frame_shadow_top"));

      // Shadow edge left (spans full height)
      div(ctx, mk(entity, 2),
          ComponentConfig{}
              .with_size(ComponentSize{edge, pixels(h)})
              .with_absolute_position()
              .with_translate(0.0f, 0.0f)
              .with_custom_background(shadow_color)
              .with_skip_tabbing(true)
              .with_debug_name("frame_shadow_left"));

      // Highlight edge bottom (spans full width)
      div(ctx, mk(entity, 3),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(w), edge})
              .with_absolute_position()
              .with_translate(0.0f, h - edge_px)
              .with_custom_background(highlight_color)
              .with_skip_tabbing(true)
              .with_debug_name("frame_highlight_bottom"));

      // Highlight edge right (spans full height)
      div(ctx, mk(entity, 4),
          ComponentConfig{}
              .with_size(ComponentSize{edge, pixels(h)})
              .with_absolute_position()
              .with_translate(w - edge_px, 0.0f)
              .with_custom_background(highlight_color)
              .with_skip_tabbing(true)
              .with_debug_name("frame_highlight_right"));
    }

    // Inner background
    Size inset = pixels(frame_thickness);
    div(ctx, mk(entity, 5),
        ComponentConfig{}
            .with_size(fill_size)
            .with_absolute_position()
            .with_translate(inset, inset)
            .with_margin(Margin{.top = inset,
                                .bottom = inset,
                                .left = inset,
                                .right = inset})
            .with_custom_background(bg_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_bg"));
  }

  return ElementResult{false, entity};
}

/// Creates a scrollable container that clips content and handles mouse wheel
/// input.
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param config Component configuration (size defines the viewport)
///
/// Features:
/// - Vertical scrolling via mouse wheel when hovered
/// - Content clipping to viewport bounds
/// - Automatic content size detection from children
///
/// Usage:
/// ```cpp
/// auto scroll = scroll_view(ctx, mk(parent),
///     ComponentConfig{}.with_size(pixels(300), pixels(200)));
///
/// // Add children - they can exceed the viewport height
/// for (int i = 0; i < 20; i++) {
///   div(ctx, mk(scroll.ent(), i),
///       ComponentConfig{}.with_size(percent(1.0f), pixels(40))
///                        .with_label(fmt::format("Item {}", i)));
/// }
/// ```
ElementResult scroll_view(HasUIContext auto &ctx, EntityParent ep_pair,
                          ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Default to children-sized container if no size specified
  if (config.size.is_default) {
    config.with_size(ComponentSize{percent(1.0f), percent(1.0f)});
  }

  // Ensure column layout for vertical scrolling
  if (config.flex_direction == FlexDirection::None) {
    config.with_flex_direction(FlexDirection::Column);
  }

  _init_component(ctx, ep_pair, config, ComponentType::Div, false,
                  "scroll_view");

  // Initialize or get scroll state
  auto &scroll_state = entity.template addComponentIfMissing<HasScrollView>();

  // Update viewport size from computed layout
  UIComponent &cmp = entity.template get<UIComponent>();
  scroll_state.viewport_size = {cmp.computed[Axis::X], cmp.computed[Axis::Y]};

  // Handle mouse wheel input when mouse is inside the scroll view
  // Note: rect uses computed values from the previous frame's layout.
  // On the first frame, rect may be empty - we skip input handling then.
  Rectangle rect = cmp.rect();
  bool has_valid_rect = rect.width > 0.0f && rect.height > 0.0f;
  bool mouse_inside = has_valid_rect && is_mouse_inside(ctx.mouse.pos, rect);

  bool scrolled = false;
  if (mouse_inside) {
    // Use wheel vector for both vertical and horizontal scrolling
    // Trackpads and some mice provide both axes
    Vector2Type wheel_v = input::get_mouse_wheel_move_v();

    // Direction multiplier: natural scrolling (default) vs inverted
    float direction = scroll_state.invert_scroll ? 1.0f : -1.0f;

    // Vertical scrolling
    if (scroll_state.vertical_enabled && wheel_v.y != 0.0f) {
      float old_offset = scroll_state.scroll_offset.y;
      scroll_state.scroll_offset.y +=
          direction * wheel_v.y * scroll_state.scroll_speed;
      // Prevent scrolling past the start (full clamping happens in render
      // after content_size is computed)
      if (scroll_state.scroll_offset.y < 0.0f) {
        scroll_state.scroll_offset.y = 0.0f;
      }
      scrolled = scrolled || (scroll_state.scroll_offset.y != old_offset);
    }

    // Horizontal scrolling
    if (scroll_state.horizontal_enabled && wheel_v.x != 0.0f) {
      float old_offset = scroll_state.scroll_offset.x;
      scroll_state.scroll_offset.x +=
          direction * wheel_v.x * scroll_state.scroll_speed;
      // Prevent scrolling past the start
      if (scroll_state.scroll_offset.x < 0.0f) {
        scroll_state.scroll_offset.x = 0.0f;
      }
      scrolled = scrolled || (scroll_state.scroll_offset.x != old_offset);
    }
  }

  return {scrolled, entity};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
