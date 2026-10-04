#pragma once

#include "../color.h"
#include "imm_components.h"
#include "text_input/text_input.h"

#include <array>
#include <string>

namespace afterhours::ui {
struct HasColorSwatchState : BaseComponent {
  std::string hex_text;
  EntityID hex_id = -1;
};
} // namespace afterhours::ui

namespace afterhours::ui::imm {

namespace color_swatch_detail {
inline float row_height(HasUIContext auto &ctx, const ComponentConfig &config) {
  const float px = resolve_to_pixels(config.size.y_axis, ctx.screen_width,
                                     ctx.screen_height);
  // Fall back only when the height did not resolve; a small positive
  // height is the caller's choice and passes through.
  return std::max(px, 28.f * static_cast<float>(px <= 0.f));
}

inline float button_pad(HasUIContext auto &ctx, Axis axis) {
  Size pad = spacing_to_size(Spacing::sm);
  if (const auto *defaults = UIStylingDefaults::get().find_component_config(
          ComponentType::Button)) {
    const Size side =
        axis == Axis::X ? defaults->padding.left : defaults->padding.top;
    if (side.dim != Dim::None)
      pad = side;
  }
  return resolve_to_pixels(
      pad, ctx.screen_height,
      ctx.scaling_mode.value_or(UIStylingDefaults::get().scaling_mode),
      ctx.theme.ui_scale, ctx.screen_height);
}

inline void draw_checker(RectangleType r, Color dark, Color light) {
  const float cell = std::max(2.f, r.height / 5.f);
  afterhours::draw_rectangle(r, dark);
  for (float y = r.y; y < r.y + r.height; y += cell) {
    const int row = static_cast<int>((y - r.y) / cell);
    for (float x = r.x + (row % 2 ? cell : 0.f); x < r.x + r.width;
         x += cell * 2.f) {
      const float w = std::min(cell, r.x + r.width - x);
      const float h = std::min(cell, r.y + r.height - y);
      afterhours::draw_rectangle(RectangleType{x, y, w, h}, light);
    }
  }
}

bool channel_row(HasUIContext auto &ctx, EntityParent row_pair,
                 const std::string &name, const std::string &letter,
                 const std::string &readout, float &channel01,
                 const ComponentConfig &config, float item_h) {
  auto row = div(ctx, row_pair,
                 ComponentConfig::inherit_from(config, "color_channel_row")
                     .with_size(ComponentSize{percent(1.f), pixels(item_h)})
                     .with_flex_direction(FlexDirection::Row)
                     .with_align_items(AlignItems::Center)
                     .with_transparent_bg()
                     .without_border());
  div(ctx, mk(row.ent(), 0),
      ComponentConfig::inherit_from(config, "color_channel_label")
          .with_size(ComponentSize{percent(0.07f), pixels(item_h)})
          .with_label(letter)
          .with_transparent_bg()
          .without_border()
          .with_skip_tabbing(true));
  // Percent widths across the row, not expand() for the slider: a slider
  // handle is a percent of its track, and a percent under an expand()-sized
  // ancestor is left at zero, which would take the handle with it.
  const float desired = channel01;
  auto result =
      slider(ctx, mk(row.ent(), 1), channel01,
             ComponentConfig::inherit_from(config, name)
                 .with_size(ComponentSize{percent(0.78f), pixels(item_h)}));
  const bool changed = static_cast<bool>(result);
  if (!changed) {
    // Nothing interacted since the last build, so an outside edit (the hex
    // field below) is newer than the slider's held state. Restore the
    // caller's value and move the held state to match, or the next drag
    // would step from the stale position.
    channel01 = desired;
    if (result.ent().template has<UIComponent>()) {
      for (const EntityID child_id :
           result.ent().template get<UIComponent>().children) {
        Entity &child = UICollectionHolder::getEntityForIDEnforce(child_id);
        if (child.template has<HasSliderState>())
          child.template get<HasSliderState>().value =
              std::clamp(desired, 0.f, 1.f);
      }
    }
  }
  div(ctx, mk(row.ent(), 2),
      ComponentConfig::inherit_from(config, "color_channel_value")
          .with_size(ComponentSize{percent(0.15f), pixels(item_h)})
          .with_label(readout)
          .with_alignment(TextAlignment::Right)
          .with_transparent_bg()
          .without_border()
          .with_skip_tabbing(true));
  return changed;
}

bool hex_field(HasUIContext auto &ctx, EntityParent row_pair,
               HasColorSwatchState &state, Color &value,
               const ComponentConfig &config, float item_h) {
  if (ctx.focus_id != state.hex_id)
    state.hex_text = colors::to_hex_string(value);

  auto row = div(ctx, row_pair,
                 ComponentConfig::inherit_from(config, "color_hex_row")
                     .with_size(ComponentSize{percent(1.f), pixels(item_h)})
                     .with_flex_direction(FlexDirection::Row)
                     .with_align_items(AlignItems::Center)
                     .with_transparent_bg()
                     .without_border());
  div(ctx, mk(row.ent(), 0),
      ComponentConfig::inherit_from(config, "color_hex_hash")
          .with_size(ComponentSize{pixels(20.f), pixels(item_h)})
          .with_label("#")
          .with_transparent_bg()
          .without_border()
          .with_skip_tabbing(true));
  auto field = text_input(
      ctx, mk(row.ent(), 1), state.hex_text,
      ComponentConfig::inherit_from(config, "color_hex_field")
          .with_size(ComponentSize{expand(), pixels(item_h)}));
  state.hex_id = field.id();
  if (!field)
    return false;
  const auto parsed = colors::parse_hex(state.hex_text);
  if (!parsed.has_value() || (parsed->r == value.r && parsed->g == value.g &&
                              parsed->b == value.b && parsed->a == value.a))
    return false;
  value = *parsed;
  return true;
}
} // namespace color_swatch_detail

ElementResult color_swatch_button(HasUIContext auto &ctx, EntityParent ep_pair,
                                  Color value,
                                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const float item_h = color_swatch_detail::row_height(ctx, config);
  const float pad_left = color_swatch_detail::button_pad(ctx, Axis::X);
  const float pad_top = color_swatch_detail::button_pad(ctx, Axis::Y);
  const float chip_w = item_h * 1.5f;
  const float chip_h = std::max(10.f, item_h - 12.f);

  auto btn = button(ctx, ep_pair,
                    ComponentConfig(config)
                        .with_label("#" + colors::to_hex_string(
                                              value, value.a < 255))
                        .with_alignment(TextAlignment::Left)
                        .with_text_inset(pad_left + chip_w + 8.f, 0.f));

  auto chip_config = ComponentConfig::inherit_from(config, "color_chip")
                         .with_size(ComponentSize{pixels(chip_w),
                                                  pixels(chip_h)})
                         .with_absolute_position(
                             0.f, (item_h - chip_h) * 0.5f - pad_top)
                         .with_corner_radius(4.f)
                         .with_custom_background(value)
                         .with_ignore_pointer_events()
                         .with_skip_tabbing(true)
                         .with_render_layer(config.render_layer + 1);
  if (value.a < 255) {
    const Color dark = ctx.theme.from_usage(Theme::Usage::Background);
    const Color light = ctx.theme.from_usage(Theme::Usage::Surface);
    chip_config.with_on_draw_bg([dark, light](RectangleType r) {
      color_swatch_detail::draw_checker(r, dark, light);
    });
  }
  div(ctx, mk(btn.ent(), 0), chip_config);
  return btn;
}

ElementResult color_channel_rows(HasUIContext auto &ctx,
                                 EntityParent ep_pair, Color &value,
                                 ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const float item_h = color_swatch_detail::row_height(ctx, config);

  auto container =
      div(ctx, ep_pair,
          ComponentConfig::inherit_from(config, "color_channels")
              .with_size(ComponentSize{config.size.x_axis, children(item_h)})
              .with_flex_direction(FlexDirection::Column)
              .with_transparent_bg()
              .without_border());

  colors::HSV hsv = colors::to_hsv(value);
  float channels[4] = {hsv.h / 360.f, hsv.s, hsv.v, value.a / 255.f};
  static constexpr std::array<const char *, 4> names = {
      "color_channel_h", "color_channel_s", "color_channel_v",
      "color_channel_a"};
  static constexpr std::array<const char *, 4> letters = {"H", "S", "V", "A"};
  const std::array<std::string, 4> readouts = {
      std::to_string(static_cast<int>(std::round(hsv.h))),
      fmt::format("{}%", static_cast<int>(std::round(hsv.s * 100.f))),
      fmt::format("{}%", static_cast<int>(std::round(hsv.v * 100.f))),
      fmt::format("{}%",
                  static_cast<int>(std::round(channels[3] * 100.f)))};

  bool changed = false;
  for (int i = 0; i < 4; i++) {
    const size_t idx = static_cast<size_t>(i);
    changed = color_swatch_detail::channel_row(
                  ctx, mk(container.ent(), i), names[idx], letters[idx],
                  readouts[idx], channels[idx], config, item_h) ||
              changed;
  }
  if (changed) {
    hsv.h = channels[0] * 360.f;
    hsv.s = channels[1];
    hsv.v = channels[2];
    value = colors::from_hsv(
        hsv, static_cast<unsigned char>(std::round(channels[3] * 255.f)));
  }

  if (entity.is_missing<HasColorSwatchState>())
    entity.addComponent<HasColorSwatchState>();
  changed = color_swatch_detail::hex_field(
                ctx, mk(container.ent(), 4),
                entity.template get<HasColorSwatchState>(), value, config,
                item_h) ||
            changed;

  return ElementResult{changed, entity};
}

ElementResult color_swatch_inline(HasUIContext auto &ctx,
                                  EntityParent ep_pair, Color &value,
                                  bool &open,
                                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const float item_h = color_swatch_detail::row_height(ctx, config);

  auto container =
      div(ctx, ep_pair,
          ComponentConfig::inherit_from(config, "color_swatch_inline")
              .with_size(ComponentSize{config.size.x_axis, children(item_h)})
              .with_flex_direction(FlexDirection::Column)
              .with_transparent_bg()
              .without_border());

  auto button_config = ComponentConfig(config).with_size(
      ComponentSize{percent(1.f), pixels(item_h)});
  if (color_swatch_button(ctx, mk(container.ent(), 0), value, button_config) &&
      !config.disabled)
    open = !open;

  bool changed = false;
  if (open)
    changed = static_cast<bool>(
        color_channel_rows(ctx, mk(container.ent(), 1), value, config));
  return ElementResult{changed, entity};
}

} // namespace afterhours::ui::imm
