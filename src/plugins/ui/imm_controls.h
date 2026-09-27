#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <string>
#include <utility>

#include "../../ecs.h"
#include "overlay.h"
#include "../input_system.h"
#include "component_init.h"
#include "components.h"
#include "element_result.h"
#include "measure_config.h"
#include "entity_management.h"
#include "fmt/format.h"
#include "rendering.h"
#include "rounded_corners.h"

#include "imm_primitives.h"
#include "imm_layout.h"

namespace afterhours {

namespace ui {

namespace imm {

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Outline & Ghost: transparent bg, readable text; Outline also adds a border
  if (config.button_variant != ButtonVariant::Filled) {
    Color original_bg = config.resolve_background_color(ctx.theme);
    config.with_custom_background(colors::transparent())
        .with_auto_text_color(false)
        .with_custom_text_color(ctx.theme.font);

    if (config.button_variant == ButtonVariant::Outline &&
        !config.has_border()) {
      config.with_border(original_bg, h720(2.0f));
    }
  }

  init_component(ctx, ep_pair, config, ComponentType::Button, true, "button");

  // Apply flex-direction specifically for buttons so they can drive wrapping
  // TODO: this is a hack to get buttons to wrap. We should find a better way
  // to do this.
  entity.get<UIComponent>().flex_direction = config.flex_direction;

  // For icon+text buttons, create child elements for icon and label
  if (config.has_icon()) {
    // Set up row layout for icon + text
    entity.get<UIComponent>().flex_direction =
        (config.icon_position == IconPosition::Left ||
         config.icon_position == IconPosition::Right)
            ? FlexDirection::Row
            : FlexDirection::Column;

    float icon_w = config.icon_source_rect->width;
    float icon_h = config.icon_source_rect->height;
    // Native size means a 256px icon in a 46px button, covering the label.
    // ScreenPercent counts as a known height too: with_720p_size is the
    // spelling responsive buttons use, and gating on Pixels alone meant those
    // were exactly the ones that got a native-size icon.
    const Dim ydim = config.size.y_axis.dim;
    if ((ydim == Dim::Pixels || ydim == Dim::ScreenPercent) && icon_h > 0.f) {
      float screen_height = 720.f;
      if (auto *pcr = EntityHelper::get_singleton_cmp<
              window_manager::ProvidesCurrentResolution>()) {
        screen_height = static_cast<float>(pcr->current_resolution.height);
      }
      const float button_h = resolve_to_pixels(config.size.y_axis,
                                               screen_height);
      const float fit = button_h * 0.6f;
      icon_w *= fit / icon_h;
      icon_h = fit;
    }

    auto make_icon = [&](int child_id) {
      sprite(ctx, mk(entity, child_id), config.icon_texture.value(),
             config.icon_source_rect.value(),
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(icon_w), pixels(icon_h)})
                 .with_self_align(SelfAlign::Center)
                 .with_skip_tabbing(true)
                 .with_debug_name("btn_icon"));
    };

    // The label is not a child, so child index cannot order it against one.
    make_icon(0);
    entity.get<UIComponent>().justify_content =
        config.icon_position == IconPosition::Right ? JustifyContent::FlexEnd
                                                    : JustifyContent::FlexStart;
  }

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

  init_component(ctx, ep_pair, config, ComponentType::ButtonGroup, false,
                 "button_group");

  // For Row: divide width among buttons
  // For Column: use percent(1.0f) width to fill parent (avoids hardcoded 200px)
  if (config.flex_direction == FlexDirection::Row) {
    if (max_width.dim == Dim::Pixels) {
      config.size.x_axis = pixels(max_width.value / labels.size());
    } else if (max_width.dim == Dim::Percent ||
               max_width.dim == Dim::ScreenPercent) {
      config.size.x_axis = max_width;
      config.size.x_axis.value /= labels.size();
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


ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                       bool &value,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  HasCheckboxState &state =
      init_state<HasCheckboxState>(entity, [&](auto &) {}, value);
  state.on = value;

  const auto indicator_alignment = config.label_alignment;
  auto label = config.label;
  config.label = "";

  // Row layout for label + toggle side-by-side
  config.with_flex_direction(FlexDirection::Row)
      .with_align_items(AlignItems::Center)
      .with_no_wrap();
  init_component(ctx, ep_pair, config, ComponentType::Div, false,
                 "checkbox_row");

  entity.template addComponentIfMissing<FocusClusterRoot>();

  // Resolve responsive defaults for sizing
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
  bool label_clicked = false;
  bool user_specified_corners = config.rounded_corners.has_value();

  // Helper: resolve color_usage for child components
  auto apply_color = [&](ComponentConfig &child_config) {
    if (config.color_usage == Theme::Usage::Default) {
      child_config.with_color_usage(Theme::Usage::Primary);
    } else {
      child_config.with_color_usage(config.color_usage);
      if (config.color_usage == Theme::Usage::Custom &&
          config.custom_color.has_value()) {
        child_config.with_custom_background(config.custom_color.value());
      }
    }
  };

  if (has_label_child) {
    config.size = config.size.scale_x(0.5f);

    auto label_config =
        ComponentConfig::inherit_from(
            config, fmt::format("checkbox label {}", config.debug_name))
            .with_size(config.size)
            .with_label(label);
    apply_color(label_config);

    if (!user_specified_corners) {
      label_config.with_rounded_corners(RoundedCorners().right_sharp());
    }

    Entity &label_ent = div(ctx, mk(entity), label_config).ent();
    label_ent.template addComponentIfMissing<InFocusCluster>();
    // A checkbox's label is part of its hit target; only the indicator was.
    if (config.disabled) {
      label_ent.template removeComponentIfExists<HasClickListener>();
    } else {
      label_ent.template addComponentIfMissing<HasClickListener>(
          [](Entity &) {});
      // A click listener is what makes a thing focusable, and the indicator
      // beside it is already the tab stop for this checkbox.
      label_ent.template addComponentIfMissing<SkipWhenTabbing>();
      if (label_ent.template get<HasClickListener>().down) {
        state.on = !state.on;
        label_clicked = true;
      }
    }
  }

  // Build toggle button config with checkbox visual defaults
  auto toggle_config =
      ComponentConfig::inherit_from(
          config, fmt::format("checkbox indiv from {}", config.debug_name))
          .with_size(config.size);
  apply_color(toggle_config);
  // The row's height is the caller's intent: a fixed size, or the
  // preferred value of a children() height. The toggle is a button, so
  // its own height comes from its label plus theme button padding,
  // which exceeds that intent and overflows the row (kart's "Layout
  // overflow" warning, child 76.7px in a 50px row). Pin it to the
  // intent; a pure children() intent (no value) leaves it auto.
  if ((config.size.y_axis.dim == Dim::Children ||
       config.size.y_axis.dim == Dim::Pixels) &&
      config.size.y_axis.value > 0.f)
    toggle_config.with_size(
        {toggle_config.size.x_axis, pixels(config.size.y_axis.value)});
  toggle_config.label_alignment = indicator_alignment;
  toggle_config.text_color_usage = config.text_color_usage;
  toggle_config.text_inset = config.text_inset;

  if (!user_specified_corners) {
    if (has_label_child) {
      toggle_config.with_rounded_corners(RoundedCorners().left_sharp());
    }
    // No-label case: keep default rounded corners from theme (no override)
  }

  if (!toggle_config.has_text_color_override()) {
    toggle_config.with_auto_text_color(true);
  }

  auto toggle_pair = mk(entity);
  auto &indicator = deref(toggle_pair).first;
  indicator.template addComponentIfMissing<HasLabel>();
  auto toggle_result =
      primitive::toggle_button(ctx, toggle_pair, toggle_config, state.on);
  indicator.template addComponentIfMissing<InFocusCluster>();
  const auto &text = state.on ? config.checkbox_checked_indicator
                              : config.checkbox_unchecked_indicator;
  indicator.template get<HasLabel>().set_label(text.value_or(""));
  if (state.on && !text.has_value())
    indicator.template addComponentIfMissing<HasCheckboxMark>();
  else
    indicator.template removeComponentIfExists<HasCheckboxMark>();

  if (toggle_result || label_clicked) {
    state.changed_since = true;
  }

  value = state.on;
  ElementResult result{state.changed_since, entity, value};
  state.changed_since = false;
  return result;
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
  init_component(ctx, ep_pair, config, ComponentType::CheckboxGroup, false,
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
  bool changed = false;

  // Wrap in a tray for single-tab-stop, arrow-key navigation
  auto t =
      tray(ctx, ep_pair,
           ComponentConfig{}
               .with_size(config.size.x_axis.value > 0
                              ? ComponentSize{config.size.x_axis, children()}
                              : ComponentSize{percent(1.0f), children()})
               .with_flex_direction(FlexDirection::Column)
               .with_debug_name(config.debug_name.empty()
                                    ? "radio_tray"
                                    : config.debug_name));

  // Circle dimensions - use MIN_TOUCH_TARGET for accessible touch area
  // The visual circle is smaller but the touch target meets accessibility
  // requirements
  constexpr float touch_target_sz = MIN_TOUCH_TARGET;
  constexpr float visual_circle_sz = 24.0f; // Visual size of the radio circle
  constexpr float dot_sz = 14.0f;
  constexpr float border_w = 2.0f;

  for (size_t i = 0; i < N; ++i) {
    bool is_selected = (i == selected_index);

    // Row button - transparent, for click handling
    // Ensure minimum touch target height
    auto row_size = config.size;
    if (row_size.y_axis.dim == Dim::Pixels &&
        row_size.y_axis.value < touch_target_sz) {
      row_size.y_axis = pixels(touch_target_sz);
    }
    auto row = button(ctx, mk(t.ent(), 100 + i),
                      ComponentConfig{}
                          .with_size(row_size)
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

    // Outer circle ring - visual element centered within touch target
    Color ring_color = is_selected ? ctx.theme.accent : ctx.theme.font_muted;
    auto ring = div(ctx, mk(row.ent(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(visual_circle_sz),
                                                 pixels(visual_circle_sz)})
                        .with_custom_background(ctx.theme.background)
                        .with_border(ring_color, border_w)
                        .with_rounded_corners(RoundedCorners().all_round())
                        .with_roundness(1.0f)
                        .with_margin(Margin{.right = pixels(10)})
                        .with_skip_tabbing(true)
                        .with_debug_name(fmt::format("radio_ring_{}", i)));

    // Inner filled dot when selected
    if (is_selected) {
      float offset = (visual_circle_sz - dot_sz) / 2.0f;
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
    } else {
      // Subtle dash for unselected state (non-color differentiation)
      float dash_w = visual_circle_sz * 0.4f;
      float dash_h = 2.0f;
      float dash_x = (visual_circle_sz - dash_w) / 2.0f;
      float dash_y = (visual_circle_sz - dash_h) / 2.0f;
      div(ctx, mk(ring.ent(), 0),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(dash_w), pixels(dash_h)})
              .with_absolute_position()
              .with_translate(dash_x, dash_y)
              .with_custom_background(ctx.theme.font_muted)
              .with_skip_tabbing(true)
              .with_debug_name(fmt::format("radio_dash_{}", i)));
    }

    // Label - positioned after circle
    auto label_ent =
        div(ctx, mk(row.ent(), 1),
            ComponentConfig{}
                .with_size(ComponentSize{pixels(150), row_size.y_axis})
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

  return {changed, t.ent(), static_cast<int>(selected_index)};
}


/// iOS-style pill toggle switch with sliding knob.
///
/// For a simple circle toggle with check/X indicator, use checkbox()
/// with .with_rounded_corners(RoundedCorners().all_round()).
ElementResult toggle_switch(HasUIContext auto &ctx, EntityParent ep_pair,
                            bool &value,
                            ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto label = config.label;
  config.label = "";

  // Ensure toggle row uses Row layout to place label and toggle side-by-side
  if (config.flex_direction == FlexDirection::Column) {
    config.with_flex_direction(FlexDirection::Row);
  }
  config.with_align_items(AlignItems::Center);
  config.with_justify_content(JustifyContent::FlexStart);
  init_component(ctx, ep_pair, config, ComponentType::ToggleSwitch, false,
                 "toggle_switch_row");

  // Add FocusClusterRoot to container for consistent focus ring on entire row
  entity.template addComponentIfMissing<FocusClusterRoot>();

  HasToggleSwitchState &state =
      init_state<HasToggleSwitchState>(entity, [&](auto &) {}, value);

  // Animate (smooth lerp toward target)
  float target = state.on ? 1.0f : 0.0f;
  state.animation_progress += (target - state.animation_progress) * 0.2f;

  const Theme &theme = ctx.theme;

  // Dimensions — declared early so label width can reference track_w
  constexpr float track_w = 52.0f, track_h = 28.0f;
  constexpr float pad = 4.0f;
  constexpr float knob_sz = track_h - pad * 2.0f;          // 20px
  constexpr float travel = track_w - knob_sz - pad * 2.0f; // 24px

  // The label takes whatever the track leaves. This used to be computed by
  // hand as parent_width - track_w - padding, to dodge expand() resolving
  // wrong; that was grid snapping rounding an expander up past its space, and
  // it is fixed, so let layout do the arithmetic.
  if (!label.empty()) {
    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config, "toggle_label")
            .with_size(ComponentSize{expand(), config.size.y_axis})
            .with_label(label)
            .with_color_usage(Theme::Usage::None))
        .ent()
        .template addComponentIfMissing<InFocusCluster>();
  }

  // Pill style (iOS-like sliding knob)
  // knob_sz = track_h - 2*pad ensures the knob circle fits perfectly
  // inside the capsule's rounded ends (centers coincide).

  // Colors — visible gray for OFF, accent for ON
  Color track_off = colors::lerp(theme.font_muted,
                                 colors::lighten(theme.background, 0.3f), 0.4f);
  Color track_on = theme.accent;
  Color track_color =
      colors::lerp(track_off, track_on, state.animation_progress);

  // Track toggle — skip_hover_override keeps correct color on hover
  // Explicit zero padding prevents default Spacing::sm button padding
  // which would offset the absolute-positioned knob outside the capsule.
  auto track_btn = primitive::toggle_button(
      ctx, mk(entity),
      ComponentConfig::inherit_from(config, "toggle_track")
          .with_size(ComponentSize{pixels(track_w), pixels(track_h)})
          .with_padding(Padding{.top = pixels(0),
                                .left = pixels(0),
                                .bottom = pixels(0),
                                .right = pixels(0)})
          .with_custom_background(track_color)
          .with_rounded_corners(RoundedCorners().all_round())
          .with_roundness(0.5f)
          .with_self_align(SelfAlign::Center),
      state.on);
  track_btn.ent().template addComponentIfMissing<InFocusCluster>();
  track_btn.ent().template get<HasColor>().skip_hover_override = true;
  bool clicked = track_btn;

  // Opt-in: the knob's position already carries state.
  if (config.toggle_on_indicator.has_value())
    div(ctx, mk(track_btn.ent(), 10),
        ComponentConfig{}
            .with_debug_name("toggle_on_indicator")
            .with_size(ComponentSize{pixels(track_w / 2.0f - pad),
                                     pixels(track_h - pad * 2.0f)})
            .with_absolute_position()
            .with_translate(pixels(pad + 1.0f), pixels(pad))
            .with_label(config.toggle_on_indicator.value())
            .with_font_size(pixels(14.f))
            .with_custom_text_color(Color{255, 255, 255, 200})
            .with_alignment(TextAlignment::Center)
            .with_skip_tabbing(true));

  if (config.toggle_off_indicator.has_value())
    div(ctx, mk(track_btn.ent(), 11),
        ComponentConfig{}
            .with_debug_name("toggle_off_indicator")
            .with_size(ComponentSize{pixels(track_w / 2.0f - pad),
                                     pixels(track_h - pad * 2.0f)})
            .with_absolute_position()
            .with_translate(pixels(track_w / 2.0f), pixels(pad))
            .with_label(config.toggle_off_indicator.value())
            .with_font_size(pixels(12.f))
            .with_custom_text_color(Color{255, 255, 255, 160})
            .with_alignment(TextAlignment::Center)
            .with_skip_tabbing(true));

  // Knob — white circle with subtle dark border for visibility
  float knob_x = pad + travel * state.animation_progress;
  div(ctx, mk(track_btn.ent()),
      ComponentConfig{}
          .with_debug_name("toggle_knob")
          .with_size(ComponentSize{pixels(knob_sz), pixels(knob_sz)})
          .with_absolute_position()
          .with_translate(pixels(knob_x), pixels(pad))
          .with_custom_background(Color{255, 255, 255, 255})
          .with_border(Color{0, 0, 0, 40}, 1.0f)
          .with_rounded_corners(RoundedCorners().all_round())
          .with_roundness(1.0f)
          .with_skip_tabbing(true));

  // toggle_button already flipped state.on if clicked
  if (clicked) {
    state.changed_since = true;
  }

  value = state.on;
  ElementResult result{state.changed_since, entity, value};
  state.changed_since = false;
  return result;
}


} // namespace imm

} // namespace ui

} // namespace afterhours
