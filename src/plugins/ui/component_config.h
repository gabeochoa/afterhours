#pragma once

#include <bitset>
#include <functional>
#include <optional>
#include <string>

#include "../autolayout.h"
#include "../color.h"
#include "../texture_manager.h"
#include "components.h"
#include "rounded_corners.h"
#include "styling_defaults.h"
#include "theme.h"

namespace afterhours {

namespace ui {

namespace imm {

struct TextureConfig {
  texture_manager::Texture texture;
  texture_manager::HasTexture::Alignment alignment =
      texture_manager::HasTexture::Alignment::None;
};

// TODO: Consider splitting ComponentConfig into concept-constrained configs per
// component type. Example: TextInputConfig concept would only expose methods
// relevant to text inputs (with_mask_char, with_max_length, etc.) while hiding
// irrelevant ones (with_dropdown_options). This would:
// - Make the API more discoverable for each component
// - Catch misconfigurations at compile time
// - Make it obvious which components need new features when adding them
// See: https://en.cppreference.com/w/cpp/language/constraints

struct ComponentConfig {
  ComponentSize size = ComponentSize(pixels(default_component_size.x),
                                     pixels(default_component_size.y), true);
  Padding padding;
  Margin margin;
  std::string label;
  bool is_absolute = false;
  FlexDirection flex_direction = FlexDirection::Column;
  JustifyContent justify_content = JustifyContent::FlexStart;
  AlignItems align_items = AlignItems::FlexStart;
  SelfAlign self_align = SelfAlign::Auto;
  FlexWrap flex_wrap = FlexWrap::Wrap;
  bool debug_wrap = false;

  // Background color settings
  Theme::Usage color_usage = Theme::Usage::Default;
  std::optional<Color> custom_color;

  // Text color settings (explicit override)
  Theme::Usage text_color_usage = Theme::Usage::Default;
  std::optional<Color> custom_text_color;

  // When enabled, text color is automatically selected for best contrast
  // against the background color (uses auto_text_color).
  // Default: true (ensures accessible text on any background)
  bool auto_text_color = true;

  std::optional<TextureConfig> texture_config;
  std::optional<texture_manager::HasTexture::Alignment> image_alignment;
  std::optional<std::bitset<4>> rounded_corners;
  std::optional<float> roundness; // If unset, uses theme.roundness
  std::optional<int> segments;    // If unset, uses theme.segments

  // TODO should everything be inheritable?
  // inheritable options
  TextAlignment label_alignment = TextAlignment::None;
  bool skip_when_tabbing = false;
  bool disabled = false;
  bool hidden = false;
  bool select_on_focus = false;

  ClickActivationMode click_activation = ClickActivationMode::Default;

  // ui modifiers
  float opacity = 1.0f;
  float translate_x = 0.0f;
  float translate_y = 0.0f;

  // debugs
  std::string debug_name = "";
  int render_layer = 0;

  std::string font_name = UIComponent::UNSET_FONT;
  float font_size = 50.f;
  bool is_internal = false;

  // Shadow configuration
  std::optional<Shadow> shadow_config;

  // Border configuration
  std::optional<Border> border_config;

  // Bevel border configuration
  std::optional<BevelBorder> bevel_config;

  // Text stroke/outline configuration
  std::optional<TextStroke> text_stroke_config;

  // Text drop shadow configuration
  std::optional<TextShadow> text_shadow_config;

  // Text input: character to display instead of actual text (for passwords)
  std::optional<char> mask_char;

  // Nine-slice border configuration
  std::optional<NineSliceBorder> nine_slice_config;

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
  ComponentConfig &with_margin(Spacing spacing) {
    auto gap_size = spacing_to_size(spacing);
    margin = Margin{.top = gap_size,
                    .bottom = gap_size,
                    .left = gap_size,
                    .right = gap_size};
    return *this;
  }
  ComponentConfig &with_padding(Spacing spacing) {
    auto gap_size = spacing_to_size(spacing);
    padding = Padding{.top = gap_size,
                      .left = gap_size,
                      .bottom = gap_size,
                      .right = gap_size};
    return *this;
  }
  ComponentConfig &with_border(Color color, float thickness = 2.0f) {
    border_config = Border{color, thickness};
    return *this;
  }
  ComponentConfig &with_bevel(const BevelBorder &bevel) {
    bevel_config = bevel;
    return *this;
  }
  ComponentConfig &with_bevel(BevelStyle style,
                              Color light = Color{255, 255, 255, 255},
                              Color dark = Color{128, 128, 128, 255},
                              float thickness = 1.0f) {
    bevel_config = BevelBorder{light, dark, thickness, style};
    return *this;
  }
  // NEW: Explicit background color APIs
  ComponentConfig &with_background(Theme::Usage usage) {
    color_usage = usage;
    return *this;
  }
  ComponentConfig &with_custom_background(Color color) {
    color_usage = Theme::Usage::Custom;
    custom_color = color;
    return *this;
  }

  // DEPRECATED: Keep for backwards compatibility
  [[deprecated("Use with_background() instead")]]
  ComponentConfig &with_color_usage(Theme::Usage usage) {
    return with_background(usage);
  }
  [[deprecated("Use with_custom_background() instead")]]
  ComponentConfig &with_custom_color(Color color) {
    return with_custom_background(color);
  }

  ComponentConfig &with_text_color(Theme::Usage usage) {
    text_color_usage = usage;
    return *this;
  }
  ComponentConfig &with_custom_text_color(Color color) {
    text_color_usage = Theme::Usage::Custom;
    custom_text_color = color;
    return *this;
  }

  ComponentConfig &with_auto_text_color(bool enabled = true) {
    auto_text_color = enabled;
    return *this;
  }
  ComponentConfig &with_mask_char(char c) {
    mask_char = c;
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
  ComponentConfig &with_roundness(float r) {
    roundness = r;
    return *this;
  }
  ComponentConfig &with_segments(int s) {
    segments = s;
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
  ComponentConfig &with_click_activation(ClickActivationMode mode) {
    click_activation = mode;
    return *this;
  }
  ComponentConfig &with_translate(float x, float y) {
    translate_x = x;
    translate_y = y;
    return *this;
  }
  ComponentConfig &with_opacity(float v) {
    opacity = v;
    return *this;
  }
  ComponentConfig &with_flex_direction(FlexDirection dir) {
    flex_direction = dir;
    return *this;
  }
  ComponentConfig &with_justify_content(JustifyContent jc) {
    justify_content = jc;
    return *this;
  }
  ComponentConfig &with_align_items(AlignItems ai) {
    align_items = ai;
    return *this;
  }
  ComponentConfig &with_self_align(SelfAlign sa) {
    self_align = sa;
    return *this;
  }
  ComponentConfig &with_no_wrap() {
    flex_wrap = FlexWrap::NoWrap;
    return *this;
  }
  ComponentConfig &with_flex_wrap(FlexWrap fw) {
    flex_wrap = fw;
    return *this;
  }
  ComponentConfig &with_debug_wrap(bool enabled = true) {
    debug_wrap = enabled;
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

  // Shadow configuration methods
  ComponentConfig &with_shadow(const Shadow &shadow) {
    shadow_config = shadow;
    return *this;
  }

  ComponentConfig &with_shadow(ShadowStyle style, float offset_x = 4.0f,
                               float offset_y = 4.0f, float blur = 8.0f,
                               Color color = Color{0, 0, 0, 80}) {
    shadow_config = Shadow{style, offset_x, offset_y, blur, color};
    return *this;
  }

  ComponentConfig &with_hard_shadow(float offset_x = 4.0f,
                                    float offset_y = 4.0f,
                                    Color color = Color{0, 0, 0, 120}) {
    shadow_config = Shadow::hard(offset_x, offset_y, color);
    return *this;
  }

  ComponentConfig &with_soft_shadow(float offset_x = 4.0f,
                                    float offset_y = 6.0f, float blur = 12.0f,
                                    Color color = Color{0, 0, 0, 60}) {
    shadow_config = Shadow::soft(offset_x, offset_y, blur, color);
    return *this;
  }

  // Text stroke/outline configuration methods
  ComponentConfig &with_text_stroke(const TextStroke &stroke) {
    text_stroke_config = stroke;
    return *this;
  }

  ComponentConfig &with_text_stroke(Color color, float thickness = 2.0f) {
    text_stroke_config = TextStroke{color, thickness};
    return *this;
  }

  bool has_text_stroke() const {
    return text_stroke_config.has_value() && text_stroke_config->has_stroke();
  }

  // Text drop shadow configuration methods
  ComponentConfig &with_text_shadow(const TextShadow &shadow) {
    text_shadow_config = shadow;
    return *this;
  }

  ComponentConfig &with_text_shadow(Color color, float offset_x = 2.0f,
                                    float offset_y = 2.0f) {
    text_shadow_config = TextShadow{color, offset_x, offset_y};
    return *this;
  }

  ComponentConfig &with_soft_text_shadow(float offset_x = 2.0f,
                                         float offset_y = 2.0f) {
    text_shadow_config = TextShadow::soft(offset_x, offset_y);
    return *this;
  }

  ComponentConfig &with_hard_text_shadow(float offset_x = 2.0f,
                                         float offset_y = 2.0f) {
    text_shadow_config = TextShadow::hard(offset_x, offset_y);
    return *this;
  }

  bool has_text_shadow() const {
    return text_shadow_config.has_value() && text_shadow_config->has_shadow();
  }

  // Nine-slice border configuration methods
  ComponentConfig &with_nine_slice_border(const NineSliceBorder &nine_slice) {
    nine_slice_config = nine_slice;
    return *this;
  }

  ComponentConfig &
  with_nine_slice_border(texture_manager::Texture texture, int slice_size = 16,
                         Color tint = Color{255, 255, 255, 255}) {
    nine_slice_config = NineSliceBorder::uniform(texture, slice_size, tint);
    return *this;
  }

  ComponentConfig &
  with_nine_slice_border(texture_manager::Texture texture, int left, int top,
                         int right, int bottom,
                         Color tint = Color{255, 255, 255, 255}) {
    nine_slice_config =
        NineSliceBorder::custom(texture, left, top, right, bottom, tint);
    return *this;
  }

  bool has_nine_slice() const { return nine_slice_config.has_value(); }

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
  bool has_shadow() const { return shadow_config.has_value(); }
  bool has_border() const { return border_config.has_value(); }
  bool has_bevel() const {
    return bevel_config.has_value() && bevel_config->has_bevel();
  }
  bool is_disabled() const { return disabled; }
  bool is_hidden() const { return hidden; }
  bool skips_when_tabbing() const { return skip_when_tabbing; }
  bool selects_on_focus() const { return select_on_focus; }
  bool has_click_activation_override() const {
    return click_activation != ClickActivationMode::Default;
  }

  ComponentConfig &apply_automatic_defaults() {
    if (!has_padding()) {
      padding = Padding{.top = DefaultSpacing::medium(),
                        .left = DefaultSpacing::medium(),
                        .bottom = DefaultSpacing::medium(),
                        .right = DefaultSpacing::medium()};
    }
    if (!has_margin()) {
      margin = Margin{.top = DefaultSpacing::small(),
                      .left = DefaultSpacing::small(),
                      .bottom = DefaultSpacing::small(),
                      .right = DefaultSpacing::small()};
    }
    if (!has_font_override()) {
      font_size = TypographyScale::BASE_SIZE_720P;
    }
    return *this;
  }

  ComponentConfig apply_overrides(const ComponentConfig &overrides) const {
    ComponentConfig merged = *this;

    if (overrides.has_padding())
      merged.padding = overrides.padding;
    if (overrides.has_margin())
      merged.margin = overrides.margin;
    if (overrides.has_size_override())
      merged.size = overrides.size;

    if (overrides.color_usage != Theme::Usage::Default) {
      merged.color_usage = overrides.color_usage;
      merged.custom_color = overrides.custom_color;
    }

    // Text color overrides
    if (overrides.text_color_usage != Theme::Usage::Default) {
      merged.text_color_usage = overrides.text_color_usage;
      merged.custom_text_color = overrides.custom_text_color;
    }

    if (overrides.has_label_alignment_override())
      merged.label_alignment = overrides.label_alignment;

    if (!overrides.label.empty())
      merged.label = overrides.label;

    if (overrides.has_any_rounded_corners())
      merged.rounded_corners = overrides.rounded_corners;

    if (overrides.is_disabled())
      merged.disabled = overrides.disabled;
    if (overrides.is_hidden())
      merged.hidden = overrides.hidden;
    if (overrides.skips_when_tabbing())
      merged.skip_when_tabbing = overrides.skip_when_tabbing;
    if (overrides.selects_on_focus())
      merged.select_on_focus = overrides.select_on_focus;
    if (overrides.has_click_activation_override())
      merged.click_activation = overrides.click_activation;

    if (overrides.has_font_override()) {
      merged.font_name = overrides.font_name;
      merged.font_size = overrides.font_size;
    }

    if (overrides.has_texture())
      merged.texture_config = overrides.texture_config;
    if (overrides.has_image_alignment())
      merged.image_alignment = overrides.image_alignment;

    if (overrides.is_absolute)
      merged.is_absolute = overrides.is_absolute;
    if (overrides.flex_direction != FlexDirection::Column)
      merged.flex_direction = overrides.flex_direction;
    if (overrides.render_layer != 0)
      merged.render_layer = overrides.render_layer;
    if (!overrides.debug_name.empty())
      merged.debug_name = overrides.debug_name;

    // Flexbox alignment properties
    if (overrides.justify_content != JustifyContent::FlexStart)
      merged.justify_content = overrides.justify_content;
    if (overrides.align_items != AlignItems::FlexStart)
      merged.align_items = overrides.align_items;
    if (overrides.self_align != SelfAlign::Auto)
      merged.self_align = overrides.self_align;
    if (overrides.flex_wrap != FlexWrap::Wrap)
      merged.flex_wrap = overrides.flex_wrap;
    if (overrides.debug_wrap)
      merged.debug_wrap = overrides.debug_wrap;

    return merged;
  }

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
    click_activation = parent.click_activation;
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

inline ComponentConfig magazine_style() {
  ComponentConfig config;
  config
      .with_padding(Padding{.top = DefaultSpacing::large(),
                            .left = DefaultSpacing::large(),
                            .bottom = DefaultSpacing::large(),
                            .right = DefaultSpacing::large()})
      .with_margin(Margin{.top = DefaultSpacing::medium(),
                          .left = DefaultSpacing::medium(),
                          .bottom = DefaultSpacing::medium(),
                          .right = DefaultSpacing::medium()});
  config.font_size = TypographyScale::BASE_SIZE_720P;
  return config;
}

inline ComponentConfig card_style() {
  ComponentConfig config;
  config
      .with_padding(Padding{.top = DefaultSpacing::medium(),
                            .left = DefaultSpacing::medium(),
                            .bottom = DefaultSpacing::medium(),
                            .right = DefaultSpacing::medium()})
      .with_margin(Margin{.top = DefaultSpacing::small(),
                          .left = DefaultSpacing::small(),
                          .bottom = DefaultSpacing::small(),
                          .right = DefaultSpacing::small()});
  config.font_size = TypographyScale::BASE_SIZE_720P;
  return config;
}

inline ComponentConfig form_style() {
  ComponentConfig config;
  config
      .with_padding(Padding{.top = DefaultSpacing::small(),
                            .left = DefaultSpacing::medium(),
                            .bottom = DefaultSpacing::small(),
                            .right = DefaultSpacing::medium()})
      .with_margin(Margin{.top = DefaultSpacing::tiny(),
                          .left = DefaultSpacing::tiny(),
                          .bottom = DefaultSpacing::tiny(),
                          .right = DefaultSpacing::tiny()});
  config.font_size = TypographyScale::BASE_SIZE_720P;
  return config;
}

inline ComponentConfig auto_spacing() {
  return ComponentConfig{}.apply_automatic_defaults();
}

} // namespace imm

} // namespace ui

} // namespace afterhours
