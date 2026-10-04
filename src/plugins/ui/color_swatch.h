#pragma once

#include "../color.h"
#include "imm_components.h"
#include "menu.h"
#include "text_input/text_input.h"

#include <array>
#include <optional>
#include <span>
#include <string>

namespace afterhours::ui {
struct HasColorSwatchState : BaseComponent {
  std::string hex_text;
  EntityID hex_id = -1;
  bool press_began_while_open = false;
  // The preset under the pointer while a preset panel is built. The
  // anchor shows it instead of the saved value, and callers can read it
  // to preview elsewhere. Preview only: the caller's value changes when
  // a preset is clicked (published), never on hover.
  std::optional<Color> hover_preview;
};

// The palette the preset grid ships with when the caller has no opinion.
// Callers pass their own span to the widget; a brand colour is then one
// more entry in the app's list, never a library edit.
inline std::span<const Color> default_color_presets() {
  static constexpr std::array<Color, 24> presets = {{
      colors::hex(0x3C659C), colors::hex(0x2E7D6B), colors::hex(0x4C9141),
      colors::hex(0xB8860B), colors::hex(0xC25E3A), colors::hex(0xB03A48),
      colors::hex(0x9B4D8F), colors::hex(0x5E4FA3), colors::hex(0x2F5D8A),
      colors::hex(0x256B5E), colors::hex(0x3E7031), colors::hex(0x8A6914),
      colors::hex(0x96422A), colors::hex(0x8A2F3A), colors::hex(0x773A6D),
      colors::hex(0x463A7D), colors::hex(0x6B7D94), colors::hex(0x5F7267),
      colors::hex(0x767A4A), colors::hex(0x8A7A52), colors::hex(0x84635A),
      colors::hex(0x7A5A64), colors::hex(0x655A78), colors::hex(0x4A4F5E),
  }};
  return presets;
}
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

inline bool same_color(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// One source for the preset panel's numbers, so the popover can size
// itself from the same layout the chips are built with.
inline constexpr int kPresetColumns = 8;

struct PresetLayout {
  int columns, rows;
  float chip_w, chip_h, gap, pad, width, height;
};

inline PresetLayout preset_layout(float item_h, size_t preset_count) {
  PresetLayout l;
  l.columns =
      std::min<int>(kPresetColumns, std::max<int>(static_cast<int>(preset_count), 1));
  l.rows = preset_count == 0
               ? 0
               : (static_cast<int>(preset_count) + l.columns - 1) / l.columns;
  l.chip_w = item_h * 0.82f;
  l.chip_h = item_h * 0.72f;
  l.gap = std::max(2.f, item_h * 0.16f);
  l.pad = std::max(4.f, item_h * 0.33f);
  l.width = 2 * l.pad + l.columns * l.chip_w + (l.columns - 1) * l.gap;
  l.height =
      2 * l.pad + l.rows * l.chip_h + item_h + l.rows * l.gap;
  return l;
}

inline bool preset_panel(HasUIContext auto &ctx, EntityParent body_pair,
                         HasColorSwatchState &state, Color &value,
                         std::span<const Color> presets,
                         const PresetLayout &layout,
                         const ComponentConfig &config, float item_h,
                         bool full_width, bool &picked) {
  picked = false;
  // Recomputed from last frame's hot chip below. The anchor was already
  // built from the previous value this frame, so a preview shows one
  // frame after the pointer lands, the same lag was_hot has everywhere.
  state.hover_preview.reset();
  // The popover already sized its panel from this layout, so the body
  // there fills it; a standalone grid takes the caller's axis instead.
  auto body_config = ComponentConfig::inherit_from(config, "color_preset_panel")
                         .with_padding(Padding::all(pixels(layout.pad)))
                         .with_gap(pixels(layout.gap))
                         .with_transparent_bg()
                         .without_border();
  body_config.with_size(full_width ? ComponentSize{percent(1.f), percent(1.f)}
                                   : ComponentSize{config.size.x_axis,
                                                   children(item_h)});
  auto body = vstack(ctx, body_pair, body_config);

  const Color focus_color = ctx.theme.from_usage(Theme::Usage::Focus);
  bool changed = false;
  for (int row_i = 0; row_i < layout.rows; row_i++) {
    auto row = div(ctx, mk(body.ent(), row_i),
                   ComponentConfig::inherit_from(config, "color_presets_row")
                       .with_size(ComponentSize{percent(1.f),
                                                pixels(layout.chip_h)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_gap(pixels(layout.gap))
                       .with_transparent_bg()
                       .without_border()
                       .with_skip_tabbing(true));
    for (int col = 0; col < layout.columns; col++) {
      const size_t idx = static_cast<size_t>(row_i * layout.columns + col);
      if (idx >= presets.size())
        break;
      const Color preset = presets[idx];
      auto chip = ComponentConfig::inherit_from(
                      config, fmt::format("color_preset_{}", idx))
                      .with_size(ComponentSize{pixels(layout.chip_w),
                                               pixels(layout.chip_h)})
                      .with_custom_background(preset)
                      .with_custom_hover_bg(colors::lighten(preset, 0.3f))
                      .with_corner_radius(4.f)
                      .with_tooltip(
                          "#" + colors::to_hex_string(preset, preset.a < 255));
      if (same_color(preset, value))
        chip.with_border(focus_color, std::max(1.f, item_h * 0.06f));
      if (preset.a < 255) {
        const Color dark = ctx.theme.from_usage(Theme::Usage::Background);
        const Color light = ctx.theme.from_usage(Theme::Usage::Surface);
        chip.with_on_draw_bg([dark, light](RectangleType r) {
          draw_checker(r, dark, light);
        });
      }
      auto chip_result = button(ctx, mk(row.ent(), col), chip);
      if (ctx.was_hot(chip_result.id()))
        state.hover_preview = preset;
      if (chip_result) {
        if (!same_color(preset, value))
          changed = true;
        value = preset;
        picked = true;
        state.hover_preview.reset();
      }
    }
  }
  changed = hex_field(ctx, mk(body.ent(), layout.rows), state, value, config,
                      item_h) ||
            changed;
  return changed;
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

ElementResult color_preset_grid(HasUIContext auto &ctx, EntityParent ep_pair,
                                Color &value, std::span<const Color> presets,
                                ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const float item_h = color_swatch_detail::row_height(ctx, config);
  const auto layout =
      color_swatch_detail::preset_layout(item_h, presets.size());

  if (entity.is_missing<HasColorSwatchState>())
    entity.addComponent<HasColorSwatchState>();
  bool picked = false;
  const bool changed = color_swatch_detail::preset_panel(
      ctx, ep_pair, entity.template get<HasColorSwatchState>(), value, presets,
      layout, config, item_h, false, picked);
  return ElementResult{changed, entity};
}

// The preset shape: the swatch button opens an anchored panel of preset
// chips with a custom hex field below. Hovering a chip previews it on
// the anchor without touching the value; clicking publishes the pick,
// writes through and closes the panel. Typing a hex writes through and
// leaves the panel open, the same as the inline editor.
ElementResult color_swatch_presets(HasUIContext auto &ctx,
                                   EntityParent ep_pair, Color &value,
                                   bool &open,
                                   std::span<const Color> presets,
                                   ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  const float item_h = color_swatch_detail::row_height(ctx, config);

  auto container =
      div(ctx, ep_pair,
          ComponentConfig::inherit_from(config, "color_swatch_presets")
              .with_size(ComponentSize{config.size.x_axis, children(item_h)})
              .with_flex_direction(FlexDirection::Column)
              .with_transparent_bg()
              .without_border());

  auto &state = entity.template addComponentIfMissing<HasColorSwatchState>();
  // A closed panel never rebuilds the preview, so drop any left over
  // from the dismiss instead of leaving the anchor showing a colour
  // that was never published.
  if (!open)
    state.hover_preview.reset();
  const Color shown = state.hover_preview.value_or(value);
  auto button_config = ComponentConfig(config).with_size(
      ComponentSize{percent(1.f), pixels(item_h)});
  auto btn =
      color_swatch_button(ctx, mk(container.ent(), 0), shown, button_config);
  const RectangleType anchor = btn.ent().template get<UIComponent>().rect();
  // The press half of a closing click is the popover's outside-press
  // dismissal. Without remembering which side of it we were on, the
  // release half lands as a fresh toggle and the anchor can never close
  // its own palette.
  // Latch, never overwrite: a press edge can stay raised into the frame
  // after dismissal already set open false, and re-recording then would
  // clear the memory this guard exists to keep.
  if (ctx.mouse.just_pressed && open && is_mouse_inside(ctx.mouse.pos, anchor))
    state.press_began_while_open = true;
  if (btn && !config.disabled && !state.press_began_while_open)
    open = !open;
  if (ctx.mouse.just_released)
    state.press_began_while_open = false;

  const auto layout =
      color_swatch_detail::preset_layout(item_h, presets.size());
  auto pop_config =
      ComponentConfig::inherit_from(config, "color_preset_popover")
          .with_size(
              ComponentSize{pixels(layout.width), pixels(layout.height)})
          .with_border(ctx.theme.from_usage(Theme::Usage::Secondary),
                       std::max(1.f, item_h * 0.04f))
          .with_corner_radius(std::max(2.f, item_h * 0.25f))
          // popover() puts its panel one layer over this config; the swatch
          // chip on each sibling button also sits one over, so claim one
          // extra here or a neighbouring chip draws through the palette.
          .with_render_layer(config.render_layer + 1);

  bool changed = false;
  if (auto pop = popover(ctx, mk(container.ent(), 1), anchor, open,
                         overlay::Placement::Below, pop_config)) {
    bool picked = false;
    changed = color_swatch_detail::preset_panel(
        ctx, mk(pop.ent(), 0), state, value, presets, layout, config, item_h,
        true, picked);
    if (picked)
      open = false;
  }
  return ElementResult{changed, entity};
}

} // namespace afterhours::ui::imm
