#pragma once

#include <bitset>
#include <functional>
#include <optional>
#include <string>

#include "../autolayout.h"
#include "../color.h"
#include "../texture_manager.h"
#include "animation_config.h"
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

enum struct ButtonVariant { Filled, Outline, Ghost };
enum struct IconPosition { Left, Right };

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
  bool clip_children = false; // Enable scissor clipping for children

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
  float scale = 1.0f;  // Visual scale applied after layout (smooth animations)
  Size translate_x = pixels(0.0f);
  Size translate_y = pixels(0.0f);

  // debugs
  std::string debug_name = "";
  int render_layer = 0;

  std::string font_name = UIComponent::UNSET_FONT;
  Size font_size = pixels(50.f);
  bool font_size_explicitly_set = false;
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

  // Checkbox indicator characters (default: "v" for checked, " " for unchecked)
  std::optional<std::string> checkbox_checked_indicator;
  std::optional<std::string> checkbox_unchecked_indicator;

  // Dropdown indicator characters (default: "v" for closed, "^" for open)
  std::optional<std::string> dropdown_open_indicator;
  std::optional<std::string> dropdown_closed_indicator;

  // Text area (multiline) configuration
  std::optional<Size> text_area_line_height;  // Line height (default: 20px)
  bool text_area_word_wrap = true;            // Enable word wrapping
  size_t text_area_max_lines = 0;             // Max lines (0 = unlimited)

  // Button variant configuration
  ButtonVariant button_variant = ButtonVariant::Filled;

  // Icon configuration for icon+text buttons
  std::optional<texture_manager::Texture> icon_texture;
  std::optional<texture_manager::Rectangle> icon_source_rect;
  IconPosition icon_position = IconPosition::Left;

  // Nine-slice border configuration
  std::optional<NineSliceBorder> nine_slice_config;

  // Animation configurations
  std::vector<AnimationDef> animations;

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
  // Float overload for backwards compatibility
  ComponentConfig &with_border(Color color, float thickness = 2.0f) {
    border_config = Border{color, pixels(thickness)};
    return *this;
  }
  // Size overload for resolution-scaled border thickness
  ComponentConfig &with_border(Color color, Size thickness) {
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
  // Button variant APIs
  ComponentConfig &with_button_variant(ButtonVariant variant) {
    button_variant = variant;
    return *this;
  }

  // Icon APIs for icon+text buttons
  ComponentConfig &with_icon(texture_manager::Texture texture,
                             texture_manager::Rectangle source_rect) {
    icon_texture = texture;
    icon_source_rect = source_rect;
    return *this;
  }
  ComponentConfig &with_icon_position(IconPosition pos) {
    icon_position = pos;
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
  ComponentConfig &with_transparent_bg() {
    return with_custom_background(Color{0, 0, 0, 0});
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
  ComponentConfig &with_checkbox_indicators(const std::string &checked, const std::string &unchecked = " ") {
    checkbox_checked_indicator = checked;
    checkbox_unchecked_indicator = unchecked;
    return *this;
  }
  ComponentConfig &with_dropdown_indicators(const std::string &closed, const std::string &open) {
    dropdown_closed_indicator = closed;
    dropdown_open_indicator = open;
    return *this;
  }

  // Text area (multiline) configuration methods
  ComponentConfig &with_line_height(Size height) {
    text_area_line_height = height;
    return *this;
  }
  ComponentConfig &with_word_wrap(bool enabled) {
    text_area_word_wrap = enabled;
    return *this;
  }
  ComponentConfig &with_max_lines(size_t max) {
    text_area_max_lines = max;
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
  // Float overload for backwards compatibility
  ComponentConfig &with_translate(float x, float y) {
    translate_x = pixels(x);
    translate_y = pixels(y);
    return *this;
  }
  // Size overload for resolution-scaled translation
  ComponentConfig &with_translate(Size x, Size y) {
    translate_x = x;
    translate_y = y;
    return *this;
  }
  ComponentConfig &with_opacity(float v) {
    opacity = v;
    return *this;
  }
  /// Apply visual scale after layout (bypasses layout recalculation).
  /// Use this for smooth scale animations instead of changing size.
  ComponentConfig &with_scale(float s) {
    scale = s;
    return *this;
  }
  /// Add a declarative animation that triggers automatically.
  /// Example: .with_animation(Anim::on_click().scale(0.9f, 1.0f).spring())
  ComponentConfig &with_animation(const Anim &anim) {
    animations.push_back(anim.build());
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
  ComponentConfig &with_clip_children(bool enabled = true) {
    clip_children = enabled;
    return *this;
  }
  ComponentConfig &with_font(const std::string &font_name_, Size font_size_) {
    font_name = font_name_;
    font_size = font_size_;
    font_size_explicitly_set = true;
    return *this;
  }

  // Float overload for backwards compatibility - converts to pixels
  ComponentConfig &with_font(const std::string &font_name_, float font_size_px) {
    return with_font(font_name_, pixels(font_size_px));
  }

  /// Set only the font size, leaving the font name to be resolved from
  /// the default font. Use this when you want a custom size but the
  /// theme/default font name.
  ComponentConfig &with_font_size(Size font_size_) {
    font_size = font_size_;
    font_size_explicitly_set = true;
    return *this;
  }

  /// Float overload for backwards compatibility - converts to pixels
  ComponentConfig &with_font_size(float font_size_px) {
    return with_font_size(pixels(font_size_px));
  }

  /// Set the font size from a theme FontSizing tier (Small/Medium/Large/XL).
  /// Resolves the pixel size from the theme's FontSizing at call time.
  /// The font name is left to be resolved from the default font.
  /// Example: .with_font_tier(FontSizing::Tier::Large)
  ComponentConfig &with_font_tier(FontSizing::Tier tier) {
    auto &theme = imm::ThemeDefaults::get().theme;
    font_size = h720(theme.font_sizing.get(tier));
    font_size_explicitly_set = true;
    return *this;
  }

  // TODO eventually rename this to is_absolute() instead 0
  ComponentConfig &with_absolute_position() {
    is_absolute = true;
    if (has_margin()) {
      log_warn("with_absolute_position() used with margins. "
               "For absolute elements, margins are position offsets only "
               "(they don't shrink the element). Consider using "
               "with_translate() for clearer intent.");
    }
    return *this;
  }
  ComponentConfig &with_absolute_position(float x, float y) {
    return with_absolute_position().with_translate(x, y);
  }
  ComponentConfig &with_absolute_position(Size x, Size y) {
    return with_absolute_position().with_translate(x, y);
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
  bool has_icon() const {
    return icon_texture.has_value() && icon_source_rect.has_value();
  }
  bool has_button_variant_override() const {
    return button_variant != ButtonVariant::Filled;
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
  bool has_font_size_override() const {
    return font_size_explicitly_set;
  }
  bool has_text_color_override() const {
    return text_color_usage != Theme::Usage::Default ||
           custom_text_color.has_value();
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

  // Resolve the effective background color from color_usage / custom_color.
  // Falls back to Primary if no usage is set.
  Color resolve_background_color(const Theme &theme) const {
    if (color_usage == Theme::Usage::Custom && custom_color.has_value()) {
      return custom_color.value();
    }
    Theme::Usage usage =
        Theme::is_valid(color_usage) ? color_usage : Theme::Usage::Primary;
    return theme.from_usage(usage, disabled);
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
      font_size = TypographyScale::base();
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
    }
    if (overrides.has_font_override() || overrides.has_font_size_override()) {
      merged.font_size = overrides.font_size;
      merged.font_size_explicitly_set = overrides.font_size_explicitly_set;
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
    font_size_explicitly_set = parent.font_size_explicitly_set;
    is_internal = parent.is_internal;
    render_layer = std::max(render_layer, parent.render_layer);
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
  config.font_size = TypographyScale::base();
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
  config.font_size = TypographyScale::base();
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
  config.font_size = TypographyScale::base();
  return config;
}

inline ComponentConfig auto_spacing() {
  return ComponentConfig{}.apply_automatic_defaults();
}

} // namespace imm

} // namespace ui

} // namespace afterhours
