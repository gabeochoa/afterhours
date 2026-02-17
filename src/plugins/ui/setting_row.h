#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "../color.h"
#include "component_config.h"
#include "element_result.h"
#include "entity_management.h"
#include "imm_components.h"

namespace afterhours {

namespace ui {

namespace imm {

/// Control type for setting row right-side widget
enum struct SettingRowControlType {
  Toggle,   // Boolean toggle switch (iOS style)
  Stepper,  // < value > arrows for cycling through options
  Slider,   // Inline slider for 0.0-1.0 values
  Display,  // Read-only value display (no interaction)
  Dropdown, // Opens dropdown (future support)
};

/// Configuration for a setting row control value
/// Use the appropriate variant based on control type:
/// - Toggle: bool*
/// - Stepper: pair<size_t*, vector<string>>
/// - Slider: float*
/// - Display: string
using SettingRowValue =
    std::variant<bool *,                                        // Toggle
                 std::pair<size_t *, std::vector<std::string>>, // Stepper
                 float *,                                       // Slider
                 std::string                                    // Display
                 >;

/// Configuration for the setting row component
struct SettingRowConfig {
  std::string label;
  SettingRowControlType control_type = SettingRowControlType::Display;

  // Optional icon configuration
  std::optional<std::string> icon_text;      // Text/symbol to show in icon
  std::optional<TextureConfig> icon_texture; // Or texture for icon
  std::optional<Color> icon_bg_color;        // Icon background color

  // Row styling
  float row_height = 44.0f;
  float icon_size = 28.0f;
  float icon_margin = 8.0f;
  float label_gap = 12.0f;
  float row_spacing = 6.0f; // Vertical gap between rows

  // Toggle-specific styling
  float toggle_track_width = 44.0f;
  float toggle_track_height = 24.0f;
  float toggle_knob_size = 20.0f;
  Color toggle_on_color{75, 195, 95, 255};  // iOS green
  Color toggle_off_color{85, 90, 100, 255}; // Gray track

  // Stepper-specific styling
  float stepper_arrow_width = 24.0f;
  float stepper_value_width = 80.0f;

  // Slider-specific styling
  float slider_width = 200.0f; // Wider to make track more usable
  float slider_height = 28.0f; // Taller to align better with label text

  // Slot config overrides (only specify what you want to change)
  // These merge with the sensible defaults - no need to specify everything
  std::optional<ComponentConfig> slot_icon_config;
  std::optional<ComponentConfig> slot_label_config;
  std::optional<ComponentConfig> slot_control_config;

  // Fluent builders
  SettingRowConfig &with_label(const std::string &lbl) {
    label = lbl;
    return *this;
  }
  SettingRowConfig &with_control_type(SettingRowControlType type) {
    control_type = type;
    return *this;
  }
  SettingRowConfig &with_icon(const std::string &text) {
    icon_text = text;
    return *this;
  }
  SettingRowConfig &with_icon_texture(const TextureConfig &tex) {
    icon_texture = tex;
    return *this;
  }
  SettingRowConfig &with_icon_bg_color(const Color &color) {
    icon_bg_color = color;
    return *this;
  }
  SettingRowConfig &with_row_height(float h) {
    row_height = h;
    return *this;
  }
  SettingRowConfig &with_toggle_colors(const Color &on_color,
                                       const Color &off_color) {
    toggle_on_color = on_color;
    toggle_off_color = off_color;
    return *this;
  }
  SettingRowConfig &with_slider_width(float w) {
    slider_width = w;
    return *this;
  }

  // Slot config builders - override specific child component styling
  SettingRowConfig &with_icon_config(const ComponentConfig &cfg) {
    slot_icon_config = cfg;
    return *this;
  }
  SettingRowConfig &with_label_config(const ComponentConfig &cfg) {
    slot_label_config = cfg;
    return *this;
  }
  SettingRowConfig &with_control_config(const ComponentConfig &cfg) {
    slot_control_config = cfg;
    return *this;
  }
};

/// Renders a labeled setting row with the pattern:
/// [Icon] Label .............. [Control]
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param row_config Configuration for the row layout and icon
/// @param value The value reference (must match control_type)
/// @param config Optional ComponentConfig overrides for the row container
/// @return ElementResult - true if value changed, entity reference
///
/// Usage:
/// ```cpp
/// // Toggle setting
/// bool music_on = true;
/// setting_row(ctx, mk(parent),
///             SettingRowConfig{}
///                 .with_label("Music")
///                 .with_control_type(SettingRowControlType::Toggle)
///                 .with_icon("~")
///                 .with_icon_bg_color(Color{85, 175, 125, 255}),
///             &music_on);
///
/// // Stepper setting
/// size_t quality_idx = 2;
/// std::vector<std::string> quality_options = {"Low", "Medium", "High",
/// "Ultra"}; setting_row(ctx, mk(parent),
///             SettingRowConfig{}
///                 .with_label("Quality")
///                 .with_control_type(SettingRowControlType::Stepper),
///             std::make_pair(&quality_idx, quality_options));
///
/// // Slider setting
/// float volume = 0.75f;
/// setting_row(ctx, mk(parent),
///             SettingRowConfig{}
///                 .with_label("Volume")
///                 .with_control_type(SettingRowControlType::Slider),
///             &volume);
/// ```
template <typename ValueT>
ElementResult setting_row(HasUIContext auto &ctx, EntityParent ep_pair,
                          const SettingRowConfig &row_config, ValueT value,
                          ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Calculate row dimensions
  float row_h = row_config.row_height;

  // Default size: fill parent width, fixed height
  if (config.size.is_default) {
    config.with_size(ComponentSize{percent(1.0f), pixels((int)row_h)});
  }

  // Add consistent spacing between rows
  config.with_margin(Margin{.bottom = pixels(row_config.row_spacing)});

  // Set appropriate font size for settings UI (larger for readability)
  config.font_size = pixels(22.0f);

  // Use row layout with SpaceBetween - label on left, control on right
  config.flex_direction = FlexDirection::Row;
  config.align_items = AlignItems::Center;
  config.justify_content = JustifyContent::SpaceBetween;

  // Initialize the row container
  init_component(ctx, ep_pair, config, ComponentType::Div, false,
                 "setting_row");

  bool changed = false;

  // ========== ICON (optional) ==========
  bool has_icon =
      row_config.icon_text.has_value() || row_config.icon_texture.has_value();

  if (has_icon) {
    // Build icon with sensible defaults
    auto icon_cfg =
        ComponentConfig{}
            .with_size(ComponentSize{pixels((int)row_config.icon_size),
                                     pixels((int)row_config.icon_size)})
            .with_margin(Margin{.left = pixels(0),
                                .right = pixels(row_config.label_gap)})
            .with_padding(Padding{}) // No padding so text fits
            .with_rounded_corners(std::bitset<4>(0b1111))
            .with_roundness(1.0f) // Circular
            .with_alignment(TextAlignment::Center)
            .with_font(config.font_name, 12.0f) // Small font for icons
            .with_debug_name("setting_row_icon");

    if (row_config.icon_bg_color.has_value()) {
      icon_cfg.with_custom_background(*row_config.icon_bg_color);
    } else {
      icon_cfg.with_background(Theme::Usage::Primary);
    }

    if (row_config.icon_text.has_value()) {
      icon_cfg.with_label(*row_config.icon_text);
      icon_cfg.with_custom_text_color(colors::UI_WHITE);
    }

    if (row_config.icon_texture.has_value()) {
      icon_cfg.with_texture(*row_config.icon_texture);
    }

    // Apply user override if provided
    if (row_config.slot_icon_config) {
      icon_cfg = icon_cfg.apply_overrides(*row_config.slot_icon_config);
    }

    div(ctx, mk(entity), icon_cfg);
  }

  // ========== LABEL ==========
  // Build label with sensible defaults
  // Use children() width so the label only takes as much space as its text.
  // SpaceBetween on the row pushes the label left and the control right.
  auto label_cfg = ComponentConfig{}
                       .with_size(ComponentSize{children(), pixels((int)row_h)})
                       .with_label(row_config.label)
                       .with_alignment(TextAlignment::Left)
                       .with_background(Theme::Usage::None)
                       .with_font(UIComponent::DEFAULT_FONT, config.font_size)
                       .with_custom_text_color(
                           config.custom_text_color.value_or(ctx.theme.font))
                       .with_debug_name("setting_row_label");

  // Apply user override if provided
  if (row_config.slot_label_config) {
    label_cfg = label_cfg.apply_overrides(*row_config.slot_label_config);
  }

  div(ctx, mk(entity), label_cfg);

  // ========== CONTROL (right side) ==========
  switch (row_config.control_type) {
  case SettingRowControlType::Toggle: {
    // Expect bool* value
    if constexpr (std::is_same_v<ValueT, bool *>) {
      // Build toggle with sensible defaults
      // Use children() sizing to let the container fit the toggle_switch's
      // internal sizing (which uses h720() for resolution scaling)
      auto toggle_cfg = ComponentConfig{}
                            .with_size(ComponentSize{children(), children()})
                            .with_debug_name("setting_row_toggle");

      // Apply user override if provided
      if (row_config.slot_control_config) {
        toggle_cfg =
            toggle_cfg.apply_overrides(*row_config.slot_control_config);
      }

      // Use the built-in toggle_switch component (iOS-style pill)
      // Add right margin so the focus ring stays inside the row container.
      toggle_cfg.with_margin(Margin{.right = pixels(4)});

      auto toggle_result = toggle_switch(ctx, mk(entity), *value, toggle_cfg);

      if (toggle_result) {
        changed = true;
      }
    }
    break;
  }

  // TODO add a component for this that we can use instead of biulding one
  case SettingRowControlType::Stepper: {
    // Expect pair<size_t*, vector<string>> value
    if constexpr (std::is_same_v<
                      ValueT, std::pair<size_t *, std::vector<std::string>>>) {
      auto stepper_cfg =
          ComponentConfig{}
              .with_size(ComponentSize{
                  pixels((int)(row_config.stepper_arrow_width * 2 +
                               row_config.stepper_value_width)),
                  pixels((int)row_h - 8)})
              .with_font(config.font_name, config.font_size)
              .with_debug_name("setting_row_stepper");

      if (row_config.slot_control_config) {
        stepper_cfg =
            stepper_cfg.apply_overrides(*row_config.slot_control_config);
      }

      if (stepper(ctx, mk(entity), value.second, *value.first, stepper_cfg))
        changed = true;
    }
    break;
  }

  case SettingRowControlType::Slider: {
    // Expect float* value
    if constexpr (std::is_same_v<ValueT, float *>) {
      // Build slider - no label means compact mode (no left label area)
      auto slider_cfg =
          ComponentConfig{}
              .with_size(ComponentSize{pixels((int)row_config.slider_width),
                                       pixels((int)row_config.slider_height)})
              .with_debug_name("setting_row_slider");

      // Apply user override if provided
      if (row_config.slot_control_config) {
        slider_cfg =
            slider_cfg.apply_overrides(*row_config.slot_control_config);
      }

      float slider_value = *value;
      // Show value on handle
      if (auto result = slider(ctx, mk(entity), slider_value, slider_cfg,
                               SliderHandleValueLabelPosition::OnHandle);
          result) {
        *value = slider_value;
        changed = true;
      }
    }
    break;
  }

  case SettingRowControlType::Display: {
    // Expect string value
    if constexpr (std::is_same_v<ValueT, std::string>) {
      // Build display with sensible defaults
      auto display_cfg =
          ComponentConfig{}
              .with_label(value)
              .with_size(ComponentSize{children(), pixels((int)row_h - 8)})
              .with_background(Theme::Usage::None)
              .with_custom_text_color(ctx.theme.font_muted)
              .with_alignment(TextAlignment::Right)
              .with_font(config.font_name, config.font_size)
              .with_debug_name("setting_row_display");

      // Apply user override if provided
      if (row_config.slot_control_config) {
        display_cfg =
            display_cfg.apply_overrides(*row_config.slot_control_config);
      }

      div(ctx, mk(entity), display_cfg);
    }
    break;
  }

  case SettingRowControlType::Dropdown: {
    // Expect pair<size_t*, vector<string>> value (same as Stepper)
    if constexpr (std::is_same_v<
                      ValueT, std::pair<size_t *, std::vector<std::string>>>) {
      size_t *idx_ptr = value.first;
      const auto &options = value.second;

      // Build dropdown with sensible defaults
      auto dropdown_cfg =
          ComponentConfig{}
              .with_size(ComponentSize{pixels(120), pixels((int)row_h - 4)})
              .with_debug_name("setting_row_dropdown");

      // Apply user override if provided
      if (row_config.slot_control_config) {
        dropdown_cfg =
            dropdown_cfg.apply_overrides(*row_config.slot_control_config);
      }

      if (auto result =
              dropdown(ctx, mk(entity), options, *idx_ptr, dropdown_cfg);
          result) {
        changed = true;
      }
    }
    break;
  }
  }

  return {changed, entity};
}

// ========== Convenience overloads ==========

/// Toggle setting row (bool value)
inline ElementResult
setting_row_toggle(HasUIContext auto &ctx, EntityParent ep_pair,
                   const std::string &label, bool &value,
                   const std::string &icon = "",
                   Color icon_color = colors::soft_green,
                   ComponentConfig config = ComponentConfig()) {
  auto row_config = SettingRowConfig{}.with_label(label).with_control_type(
      SettingRowControlType::Toggle);
  if (!icon.empty()) {
    row_config.with_icon(icon).with_icon_bg_color(icon_color);
  }
  return setting_row(ctx, ep_pair, row_config, &value, config);
}

/// Stepper setting row (index into options)
inline ElementResult setting_row_stepper(
    HasUIContext auto &ctx, EntityParent ep_pair, const std::string &label,
    size_t &option_idx, const std::vector<std::string> &options,
    const std::string &icon = "", Color icon_color = colors::soft_blue,
    ComponentConfig config = ComponentConfig()) {
  auto row_config = SettingRowConfig{}.with_label(label).with_control_type(
      SettingRowControlType::Stepper);
  if (!icon.empty()) {
    row_config.with_icon(icon).with_icon_bg_color(icon_color);
  }
  return setting_row(ctx, ep_pair, row_config,
                     std::make_pair(&option_idx, options), config);
}

/// Dropdown setting row (select from options)
inline ElementResult setting_row_dropdown(
    HasUIContext auto &ctx, EntityParent ep_pair, const std::string &label,
    size_t &option_idx, const std::vector<std::string> &options,
    const std::string &icon = "", Color icon_color = colors::soft_blue,
    ComponentConfig config = ComponentConfig()) {
  auto row_config = SettingRowConfig{}.with_label(label).with_control_type(
      SettingRowControlType::Dropdown);
  if (!icon.empty()) {
    row_config.with_icon(icon).with_icon_bg_color(icon_color);
  }
  return setting_row(ctx, ep_pair, row_config,
                     std::make_pair(&option_idx, options), config);
}

/// Slider setting row (float 0.0-1.0)
inline ElementResult
setting_row_slider(HasUIContext auto &ctx, EntityParent ep_pair,
                   const std::string &label, float &value,
                   const std::string &icon = "",
                   Color icon_color = colors::soft_red,
                   ComponentConfig config = ComponentConfig()) {
  auto row_config = SettingRowConfig{}.with_label(label).with_control_type(
      SettingRowControlType::Slider);
  if (!icon.empty()) {
    row_config.with_icon(icon).with_icon_bg_color(icon_color);
  }
  return setting_row(ctx, ep_pair, row_config, &value, config);
}

/// Display-only setting row (read-only string)
inline ElementResult
setting_row_display(HasUIContext auto &ctx, EntityParent ep_pair,
                    const std::string &label, const std::string &value,
                    const std::string &icon = "",
                    Color icon_color = colors::soft_purple,
                    ComponentConfig config = ComponentConfig()) {
  auto row_config = SettingRowConfig{}.with_label(label).with_control_type(
      SettingRowControlType::Display);
  if (!icon.empty()) {
    row_config.with_icon(icon).with_icon_bg_color(icon_color);
  }
  return setting_row(ctx, ep_pair, row_config, value, config);
}

} // namespace imm

} // namespace ui

} // namespace afterhours
