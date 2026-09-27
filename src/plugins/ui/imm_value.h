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
#include "imm_controls.h"

namespace afterhours {

namespace ui {

namespace imm {

namespace detail {

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
    Entity &child_entity = UICollectionHolder::getEntityForIDEnforce(child_id);
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
        UICollectionHolder::getEntityForIDEnforce(main_label_id);
    std::string new_text = generate_label_text(original_label, value, position);
    update_label_entity(main_label_entity, new_text);
  }
}

} // namespace detail


// The parts of a composite take a theme usage by default, but setting one
// clobbers a colour the caller inherited down: with_color_usage leaves
// custom_color populated and unread. Only fill in the default.
inline void default_color_usage(ComponentConfig &cfg, Theme::Usage usage) {
  if (!cfg.custom_color.has_value())
    cfg.with_color_usage(usage);
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
  // Use Row layout so label and background sit side-by-side, not stacked
  config.with_flex_direction(FlexDirection::Row);
  init_component(ctx, ep_pair, config, ComponentType::Slider, true, "slider");
  config.color_usage = original_color_usage;

  // Create main label (only when label is provided)
  if (!compact) {
    std::string main_label_text = detail::generate_label_text(
        original_label, owned_value, handle_label_position);
    auto label_corners = RoundedCorners(config.rounded_corners.value())
                             .sharp(TOP_RIGHT)
                             .sharp(BOTTOM_RIGHT);

    auto label_config = ComponentConfig::inherit_from(config, "slider_text")
                            .with_size(config.size)
                            .with_label(main_label_text)
                            .with_rounded_corners(label_corners)
                            .with_render_layer(config.render_layer + 0);
    if (original_color_usage == Theme::Usage::Default && !label_config.custom_color)
      label_config.with_color_usage(Theme::Usage::None);
    auto label = div(ctx, mk(entity, entity.id + 0), label_config);
    label.ent()
        .template get<UIComponent>()
        .set_desired_width(percent(0.5f))
        .set_desired_height(percent(1.f));
    label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // Create slider background
  // In compact mode, use full rounded corners; otherwise sharp on left
  auto elem_corners =
      compact ? RoundedCorners(config.rounded_corners.value())
              : RoundedCorners(config.rounded_corners.value()).left_sharp();
  ComponentSize bg_size{percent(compact ? 1.f : .5f), percent(1.f)};

  auto bg_config = ComponentConfig::inherit_from(config, "slider_background")
                       .with_size(bg_size)
                       .with_rounded_corners(elem_corners)
                       .with_render_layer(config.render_layer + 1);
  default_color_usage(bg_config, Theme::Usage::Secondary);
  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0), bg_config);

  elem.ent().template get<UIComponent>().set_desired_width(bg_size.x_axis);

  Entity &slider_bg = elem.ent();
  slider_bg.template addComponentIfMissing<InFocusCluster>();

  if (slider_bg.is_missing<ui::HasSliderState>())
    slider_bg.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = slider_bg.get<ui::HasSliderState>();

  // Create value update function
  auto apply_slider_value = [&sliderState, original_label, handle_label_position](
                                Entity &target, float new_value_pct) {
    float clamped = std::clamp(new_value_pct, 0.f, 1.f);
    if (clamped == sliderState.value)
      return;
    sliderState.value = clamped;
    sliderState.changed_since = true;

    UIComponent &cmp = target.get<UIComponent>();
    if (!cmp.children.empty()) {
      EntityID child_id = cmp.children[0];
      Entity &child = UICollectionHolder::getEntityForIDEnforce(child_id);
      UIComponent &child_cmp = child.get<UIComponent>();
      child_cmp.set_desired_margin(
          percent(sliderState.value * 0.75f), Axis::left);

      // Update labels based on position
      if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
        detail::update_handle_label(child, sliderState.value);
      } else if (handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabel ||
                 handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabelNewLine) {
        detail::update_main_label(target, original_label, sliderState.value,
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

  // Create handle - use bg_size (not original config) so handle stays within track
  const auto dim = config.size.x_axis.dim;
  const float track_val = config.size.x_axis.value * (compact ? 1.f : .5f);

  // Warn about tiny widths
  const bool tiny_width =
      (dim == Dim::Pixels && track_val < 8.0f) ||
      ((dim == Dim::Percent || dim == Dim::ScreenPercent) && track_val < 0.02f);
  if (tiny_width) {
    log_warn("slider width is very small (dim={}, value={:.4f}); slider handle "
             "may be invisible (component: {})",
             (int)dim, track_val, config.debug_name.c_str());
  }

  const Size handle_width_size = percent(0.25f);
  const Size handle_left_size = percent(owned_value * 0.75f);

  // TODO: Support custom handle height via a dedicated config field
  // (e.g. with_slider_handle_height) to allow oversized knob-style handles.
  auto handle_config =
      ComponentConfig::inherit_from(config, "slider_handle")
          .with_size(ComponentSize{handle_width_size, percent(1.f)})
          .with_absolute_position()
          .with_margin(Margin{.left = handle_left_size})
          .with_rounded_corners(config.rounded_corners.value())
          .with_debug_name("slider_handle")
          .with_render_layer(config.render_layer + 2);
  default_color_usage(handle_config, Theme::Usage::Primary);

  auto handle = div(ctx, mk(slider_bg), handle_config);
  handle.cmp()
      .set_desired_width(handle_config.size.x_axis)
      .set_desired_height(percent(1.f));
  handle.ent().template addComponentIfMissing<InFocusCluster>();

  // Add handle label if needed
  if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
    std::string handle_label_text =
        std::to_string(static_cast<int>(sliderState.value * 100));
    auto handle_label_config =
        ComponentConfig::inherit_from(config, "slider_handle_label")
            .with_label(handle_label_text)
            .with_size(ComponentSize{children(), children()})
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
  // Consumed on read, not cleared at the top of the build: the drag system runs
  // after it, so clearing first threw away the change it had just recorded and
  // any caller reading the returned bool never saw the drag at all.
  const bool changed = sliderState.changed_since;
  sliderState.changed_since = false;
  return ElementResult{changed, entity, sliderState.value};
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

  bool first_time = init_component(
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
    on_option_click(entity,
                    detail::prev_index(option_index - 1, options.size()));
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(ctx, mk(entity, child_index + i),
               ComponentConfig::inherit_from(config,
                                             fmt::format("option {}", i + 1))
                   // Share whatever the arrows leave. A fixed 150px per option
                   // meant five pages needed 850px whatever the container was.
                   .with_size(ComponentSize{expand(), config.size.y_axis})
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
    on_option_click(entity, detail::next_index(option_index, options.size()));
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
  init_component(ctx, ep_pair, config, ComponentType::Div, false,
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

  // Create background track. It fills the progress_bar entity (percent(1.0)),
  // NOT config.size — the entity is already config.size, so re-applying it to
  // the track compounds a percent size against the entity (e.g. percent(0.7)
  // becomes 0.7 * 0.7 of the parent). The fill/label below then fill the track.
  auto track_corners = config.rounded_corners.value_or(RoundedCorners().get());
  // At 100% the fill covers the track exactly; one box, not two.
  const bool full = normalized > 0.999f;
  auto track = div(ctx, mk(entity, 0),
                   ComponentConfig::inherit_from(config, "progress_track")
                       .with_size(ComponentSize{percent(1.0f), percent(1.0f)})
                       .with_color_usage(full ? Theme::Usage::Primary
                                              : Theme::Usage::Secondary)
                       .with_rounded_corners(RoundedCorners(track_corners))
                       .with_skip_tabbing(true)
                       .with_render_layer(config.render_layer));

  // Create fill bar (width based on normalized value)
  // The fill is a child of the track, so its size must be relative to the
  // track: width = percent(normalized), height = percent(1.0). Re-applying
  // config.size here would resolve against the track and compound the fraction
  // (e.g. a percent height of 0.7 * 0.7), leaving the fill shorter/narrower
  // than the track and offset from it. Pixel-sized tracks still work because
  // percent-of-track resolves to the right pixels.
  Size fill_width = percent(normalized);

  // Something to show, and not just a repaint of the whole track.
  if (normalized > 0.001f && !full) {
    auto fill_corners = RoundedCorners(track_corners);
    // If not fully filled, make right side sharp for clean edge
    if (normalized < 0.99f) {
      fill_corners.sharp(TOP_RIGHT).sharp(BOTTOM_RIGHT);
    }

    div(ctx, mk(track.ent(), 0),
        ComponentConfig::inherit_from(config, "progress_fill")
            .with_size(ComponentSize{fill_width, percent(1.0f)})
            .with_absolute_position()
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(fill_corners)
            .with_skip_tabbing(true)
            .with_render_layer(config.render_layer + 1));
  }

  // Add label on top if specified
  if (!label_text.empty()) {
    // Not absolute: the only flow child covers the track, and layer stacks it.
    div(ctx, mk(track.ent(), 1),
        ComponentConfig::inherit_from(config, "progress_label")
            .with_size(ComponentSize{percent(1.0f), percent(1.0f)})
            .with_label(label_text)
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
  init_component(ctx, ep_pair, config, ComponentType::CircularProgress, false,
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
    track_color = config.border_config->uniform_color();
    // Resolve Size to pixels for thickness
    float screen_height = 720.f;
    if (auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>()) {
      screen_height = static_cast<float>(pcr->current_resolution.height);
    }
    thickness = resolve_to_pixels(config.border_config->uniform_thickness(),
                                  screen_height);
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


/// Stepper — cycles through string options with < and > arrow buttons.
///
/// Renders: [ < ] [ current value ] [ > ]
/// With num_visible=3: [ < ] [ prev ] [ CURRENT ] [ next ] [ > ]
///
/// @param ctx The UI context
/// @param ep_pair Entity-parent pair for hierarchy
/// @param options The string options to cycle through
/// @param option_index Current selection index (mutated on arrow click)
/// @param config Optional ComponentConfig overrides
/// @param num_visible Number of items to show (1=current only,
/// 3=prev/current/next, etc.)
///        Always treated as odd; even values are clamped to the next odd
///        number.
/// @return ElementResult — true if value changed
///
/// Usage:
/// ```cpp
/// size_t idx = 0;
/// std::vector<std::string> opts = {"Low", "Medium", "High"};
/// if (stepper(ctx, mk(parent), opts, idx)) { /* changed */ }
/// // Show prev/current/next:
/// if (stepper(ctx, mk(parent), opts, idx, {}, 3)) { /* changed */ }
/// ```
template <typename Container>
ElementResult stepper(HasUIContext auto &ctx, EntityParent ep_pair,
                      const Container &options, size_t &option_index,
                      ComponentConfig config = ComponentConfig(),
                      size_t num_visible = 1) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  option_index = option_index % options.size();

  // Default size: fit children
  if (config.size.is_default) {
    config.with_size(ComponentSize{children(default_component_size.x),
                                   children(default_component_size.y)});
  }

  // Row layout: [<] [value] [>]
  config.flex_direction = FlexDirection::Row;
  config.align_items = AlignItems::Center;
  config.justify_content = JustifyContent::SpaceBetween;

  init_component(ctx, ep_pair, config, ComponentType::Stepper, false,
                 "stepper");

  // State + keyboard + focus setup
  HasStepperState &stepperState = init_state<HasStepperState>(
      entity, [&](auto &) {}, option_index, options.size());
  if (!stepperState.changed_since) stepperState.index = option_index;
  stepperState.index %= options.size();
  stepperState.num_options = options.size();

  entity.addComponentIfMissing<ui::HasLeftRightListener>(
      [](Entity &ent, int dir) {
        auto &state = ent.get<HasStepperState>();
        if (dir < 0)
          state.index = detail::prev_index(state.index, state.num_options);
        else
          state.index = detail::next_index(state.index, state.num_options);
        state.changed_since = true;
      });

  // Shared arrow button config
  const float arrow_w = 24.0f;
  auto arrow_cfg = ComponentConfig::inherit_from(config, "stepper_arrow")
            .without_border()
                       .with_size(ComponentSize{pixels(arrow_w), percent(1.0f)})
                       .with_background(Theme::Usage::None)
                       .with_custom_text_color(ctx.theme.font_muted)
                       .with_alignment(TextAlignment::Center);

  // Left arrow <
  if (button(ctx, mk(entity),
             ComponentConfig{arrow_cfg}.with_label("<").with_debug_name(
                 "stepper_left"))) {
    stepperState.index =
        detail::prev_index(stepperState.index, stepperState.num_options);
    stepperState.changed_since = true;
  }

  // Value display — render num_visible labels centered on current selection
  // num_visible=1: just current
  // num_visible=3: prev, current, next
  // num_visible=5: prev2, prev1, current, next1, next2
  // Clamp even to next odd
  if (num_visible % 2 == 0)
    num_visible += 1;
  const size_t half = num_visible / 2;

  // Wrap labels in a container so the row layout stays [<] [labels] [>]
  // When more than one label is visible (prev/current/next), add a gap so the
  // labels don't run together — the container sizes to children(), so
  // SpaceAround has no free space to distribute on its own.
  auto label_container =
      hstack(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "stepper_labels")
            .without_border()
                 .with_size(ComponentSize{children(), percent(1.0f)})
                 .with_justify_content(JustifyContent::SpaceAround)
                 .with_align_items(AlignItems::Center)
                 .with_gap(pixels(num_visible > 1 ? 12.0f : 0.0f))
                 .with_no_wrap()
                 .with_background(Theme::Usage::None)
                 .with_skip_tabbing(true)
                 .with_debug_name("stepper_labels"));

  // TODO: Make neighbor styling configurable (muted color, smaller font,
  // opacity, etc.)
  for (size_t i = 0; i < num_visible; ++i) {
    // offset from center: -half .. 0 .. +half
    int offset = static_cast<int>(i) - static_cast<int>(half);
    size_t display_idx = stepperState.index;
    if (offset < 0) {
      for (int j = 0; j < -offset; ++j)
        display_idx = detail::prev_index(display_idx, stepperState.num_options);
    } else if (offset > 0) {
      for (int j = 0; j < offset; ++j)
        display_idx = detail::next_index(display_idx, stepperState.num_options);
    }

    bool is_center = (offset == 0);
    div(ctx, mk(label_container.ent(), static_cast<int>(i)),
        ComponentConfig::inherit_from(config, "stepper_value")
            .without_border()
            .with_label(options[display_idx % options.size()])
            .with_size(ComponentSize{children(), percent(1.0f)})
            .with_background(Theme::Usage::None)
            .with_custom_text_color(is_center ? ctx.theme.font
                                              : ctx.theme.font_muted)
            .with_alignment(TextAlignment::Center)
            .with_skip_tabbing(true)
            .with_debug_name(is_center ? "stepper_value" : "stepper_neighbor"));
  }

  // Right arrow >
  if (button(ctx, mk(entity),
             ComponentConfig{arrow_cfg}.with_label(">").with_debug_name(
                 "stepper_right"))) {
    stepperState.index =
        detail::next_index(stepperState.index, stepperState.num_options);
    stepperState.changed_since = true;
  }

  // Write state back to caller
  option_index = stepperState.index;
  const bool changed = std::exchange(stepperState.changed_since, false);
  return ElementResult{changed, entity, static_cast<int>(option_index)};
}


} // namespace imm

} // namespace ui

} // namespace afterhours
