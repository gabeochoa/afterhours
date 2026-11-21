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

namespace afterhours {

namespace ui {

namespace imm {

/// Horizontal stack — a div with FlexDirection::Row preset.
///
/// Default size: percent(1.0) x children() — fills parent width, shrinks
/// to content height. Override with .with_size() if you need different
/// dimensions.
///
/// Usage:
/// ```cpp
/// // Fills parent width, height wraps content
/// auto row = hstack(ctx, mk(parent));
/// button(ctx, mk(row.ent()), ComponentConfig{}.with_label("A"));
/// button(ctx, mk(row.ent()), ComponentConfig{}.with_label("B"));
///
/// // Fixed height override
/// auto toolbar = hstack(ctx, mk(parent),
///     ComponentConfig{}.with_size(ComponentSize{percent(1.0f), pixels(48)}));
/// ```
ElementResult hstack(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  config.with_flex_direction(FlexDirection::Row);
  if (config.size.is_default) {
    config.with_size(ComponentSize{percent(1.0f), children()});
  }
  return div(ctx, ep_pair, config);
}

/// Vertical stack — a div with FlexDirection::Column preset.
///
/// Default size: percent(1.0) x children() — fills parent width, shrinks
/// to content height. Override with .with_size() if you need different
/// dimensions.
///
/// Usage:
/// ```cpp
/// // Fills parent width, height wraps content
/// auto col = vstack(ctx, mk(parent));
/// button(ctx, mk(col.ent()), ComponentConfig{}.with_label("Top"));
/// button(ctx, mk(col.ent()), ComponentConfig{}.with_label("Bottom"));
///
/// // Fixed height override
/// auto sidebar = vstack(ctx, mk(parent),
///     ComponentConfig{}.with_size(ComponentSize{pixels(200), percent(1.0f)}));
/// ```
ElementResult vstack(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  config.with_flex_direction(FlexDirection::Column);
  if (config.size.is_default) {
    config.with_size(ComponentSize{percent(1.0f), children()});
  }
  return div(ctx, ep_pair, config);
}

namespace detail {
/// Shared body for vsplit/hsplit.
template <size_t N>
std::array<ElementResult, N>
split_impl(HasUIContext auto &ctx, EntityParent ep_pair, FlexDirection dir,
           const Size (&sizes)[N], ComponentConfig config) {
  config.with_flex_direction(dir);
  // Fills its parent by default, unlike vstack/hstack which shrink-wrap.
  if (config.size.is_default)
    config.with_size(ComponentSize{percent(1.f), percent(1.f)});
  auto container = div(ctx, ep_pair, config);

  // Built in place: ElementResult holds a reference, so it is not
  // default-constructible. Braced-list expansion is left-to-right, which keeps
  // the regions in declared order.
  return [&]<size_t... I>(std::index_sequence<I...>) {
    return std::array<ElementResult, N>{
        div(ctx, mk(container.ent(), I),
            ComponentConfig{}.with_size(
                dir == FlexDirection::Column
                    ? ComponentSize{percent(1.f), sizes[I]}
                    : ComponentSize{sizes[I], percent(1.f)}))...};
  }(std::make_index_sequence<N>{});
}
} // namespace detail

/// Divide the parent into N stacked regions, returned all at once. N is
/// deduced from the size list. Sizes drive height; regions span the width.
///
/// ```cpp
/// auto [title, main, status] = vsplit(ctx, mk(entity),
///                                     {pixels(30), expand(1), pixels(30)});
/// ```
template <size_t N>
std::array<ElementResult, N>
vsplit(HasUIContext auto &ctx, EntityParent ep_pair, const Size (&sizes)[N],
       ComponentConfig config = ComponentConfig()) {
  return detail::split_impl(ctx, ep_pair, FlexDirection::Column, sizes,
                            std::move(config));
}

/// vsplit's row-major counterpart: sizes drive width, regions span the height.
///
/// ```cpp
/// auto [sidebar, content] = hsplit(ctx, mk(entity), {pixels(200), expand(1)});
/// ```
template <size_t N>
std::array<ElementResult, N>
hsplit(HasUIContext auto &ctx, EntityParent ep_pair, const Size (&sizes)[N],
       ComponentConfig config = ComponentConfig()) {
  return detail::split_impl(ctx, ep_pair, FlexDirection::Row, sizes,
                            std::move(config));
}


/// A draggable separator bar. `axis` is the direction it MOVES in: Axis::X for
/// a vertical bar between a left and right pane, Axis::Y for a horizontal one
/// between a top and bottom. Sizes itself thin across that axis and full-length
/// along the other, and sets the matching resize cursor.
///
/// Truthy on the frames it moved; `.as<float>()` is that frame's movement in
/// the same space as rect(). Delta, not position, so grabbing the bar off
/// centre does not jump it.
///
/// ```cpp
/// if (auto d = divider(ctx, mk(row.ent(), 1), Axis::X))
///   sidebar_width = std::clamp(sidebar_width + d.as<float>(), 120.f, 400.f);
/// ```
ElementResult divider(HasUIContext auto &ctx, EntityParent ep_pair, Axis axis,
                      ComponentConfig config = ComponentConfig()) {
  constexpr float default_thickness = 4.f;
  const bool moves_in_x = (axis == Axis::X);

  if (config.size.is_default)
    config.with_size(moves_in_x ? ComponentSize{pixels(default_thickness),
                                                percent(1.f)}
                                : ComponentSize{percent(1.f),
                                                pixels(default_thickness)});
  config.with_cursor(moves_in_x ? CursorType::ResizeH : CursorType::ResizeV);

  auto elem = div(ctx, ep_pair, config);
  Entity &entity = elem.ent();
  // The no-op callback is only there to make the element hit-testable; the
  // movement is read off `down` on the next build pass instead, so the caller
  // can act on it in normal control flow rather than inside a lambda.
  entity.addComponentIfMissing<HasDragListener>([](Entity &) {});

  const float moved = entity.get<HasDragListener>().down
                          ? (moves_in_x ? ctx.mouse.delta.x : ctx.mouse.delta.y)
                          : 0.f;
  return ElementResult{moved != 0.f, entity, moved};
}


/// A static rule, the non-draggable sibling of divider(): `axis` is the
/// direction the line runs, so Axis::X is a horizontal rule (full width,
/// 1px tall) and Axis::Y a vertical one. Colour is the theme's subtle
/// border unless the config sets one.
ElementResult divider_line(HasUIContext auto &ctx, EntityParent ep_pair,
                           Axis axis,
                           ComponentConfig config = ComponentConfig()) {
  const bool horizontal = (axis == Axis::X);
  if (config.size.is_default)
    config.with_size(horizontal ? ComponentSize{percent(1.f), pixels(1.f)}
                                : ComponentSize{pixels(1.f), percent(1.f)});
  if (config.color_usage == Theme::Usage::Default &&
      !config.custom_color.has_value())
    config.with_custom_background(ctx.theme.subtle_border());
  return div(ctx, ep_pair, config);
}


/// A small rounded readout chip, the value shown next to a slider or
/// stepper. Sizes to its text with 12px of horizontal padding; 28px tall.
/// Text is centred unless the caller picks an alignment.
ElementResult value_pill(HasUIContext auto &ctx, EntityParent ep_pair,
                         const std::string &text,
                         ComponentConfig config = ComponentConfig()) {
  if (!text.empty())
    config.with_label(text);
  if (config.label_alignment == TextAlignment::None)
    config.with_alignment(TextAlignment::Center);
  if (config.size.is_default)
    config.with_size(ComponentSize{children(), pixels(28.f)});
  if (!config.has_padding())
    config.with_padding(Padding::horizontal(pixels(12.f)));
  if (!config.corner_radius.has_value())
    config.with_roundness(1.f);
  if (config.color_usage == Theme::Usage::Default &&
      !config.custom_color.has_value())
    config.with_color_usage(Theme::Usage::Surface);
  config.with_align_items(AlignItems::Center)
      .with_justify_content(JustifyContent::Center);
  return div(ctx, ep_pair, config);
}


/// A compact settings column: rows added with settings_row() stack with
/// no gap; each row carries its own divider.
ElementResult settings_list(HasUIContext auto &ctx, EntityParent ep_pair,
                            ComponentConfig config = ComponentConfig()) {
  config.flex_direction = FlexDirection::Column;
  if (config.size.is_default)
    config.with_size(ComponentSize{percent(1.f), children()});
  return div(ctx, ep_pair, config);
}

/// One settings row: the label on the left, and the returned element is
/// the right-aligned value area the caller parents its control into. A
/// divider_line sits under the row.
ElementResult settings_row(HasUIContext auto &ctx, EntityParent ep_pair,
                           const std::string &label,
                           ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  config.flex_direction = FlexDirection::Column;
  if (config.size.is_default)
    config.with_size(ComponentSize{percent(1.f), children()});
  div(ctx, ep_pair, config);
  auto line = div(ctx, mk(entity, 0),
                  ComponentConfig{}
                      .with_size(ComponentSize{percent(1.f), pixels(36.f)})
                      .with_flex_direction(FlexDirection::Row)
                      .with_align_items(AlignItems::Center));
  div(ctx, mk(line.ent(), 0),
      ComponentConfig{}
          .with_label(label)
          .with_size(ComponentSize{children(), pixels(36.f)})
          .with_align_items(AlignItems::Center));
  auto value = div(ctx, mk(line.ent(), 1),
                   ComponentConfig{}
                       .with_size(ComponentSize{expand(), pixels(36.f)})
                       .with_flex_direction(FlexDirection::Row)
                       .with_justify_content(JustifyContent::FlexEnd)
                       .with_align_items(AlignItems::Center));
  divider_line(ctx, mk(entity, 1), Axis::X);
  return value;
}


namespace detail {
/// Shared body for hsplit_pane/vsplit_pane.
std::array<ElementResult, 3>
split_pane_impl(HasUIContext auto &ctx, EntityParent ep_pair, FlexDirection dir,
                float &ratio, ComponentConfig config, float min_ratio,
                float max_ratio) {
  const bool row = (dir == FlexDirection::Row);
  config.with_flex_direction(dir);
  if (config.size.is_default)
    config.with_size(ComponentSize{percent(1.f), percent(1.f)});
  auto container = div(ctx, ep_pair, config);

  ratio = std::clamp(ratio, min_ratio, max_ratio);
  const auto region_size = [row](Size along) {
    return row ? ComponentSize{along, percent(1.f)}
               : ComponentSize{percent(1.f), along};
  };

  auto first = div(ctx, mk(container.ent(), 0),
                   ComponentConfig{}.with_size(region_size(percent(ratio))));
  auto bar = divider(ctx, mk(container.ent(), 1), row ? Axis::X : Axis::Y);
  auto second = div(ctx, mk(container.ent(), 2),
                    ComponentConfig{}.with_size(region_size(expand(1.f))));

  // The container's rect is last frame's, which is all there is during the
  // build pass and is zero on the very first frame. Resize the first region in
  // place rather than waiting for the next frame, so the bar tracks the cursor.
  const Rectangle box = container.cmp().rect();
  const float extent = row ? box.width : box.height;
  if (bar && extent > 0.f) {
    // Clamped here rather than by the caller after the fact: the in place
    // resize below uses this value, so a caller clamping afterwards left the
    // layout showing the overshoot and the bar bounced every frame of a drag
    // past its limit.
    ratio = std::clamp(ratio + (bar.template as<float>() / extent), min_ratio,
                       max_ratio);
    UIComponent &cmp = first.cmp();
    if (row)
      cmp.set_desired_width(percent(ratio));
    else
      cmp.set_desired_height(percent(ratio));
  }

  return {first, bar, second};
}
} // namespace detail

/// hsplit's two-region form with a draggable divider between them. `ratio` is
/// the left region's share of the container width and is updated in place as
/// the divider moves; the right region takes whatever is left.
///
/// Returns {left, divider, right}. The divider comes back so it can be styled
/// or resized like any other element -- it is deliberately not a config knob.
/// Clamped only to [0, 1]: a pane with a minimum width should clamp `ratio`
/// itself before the next call.
///
/// ```cpp
/// auto [nav, bar, body] = hsplit_pane(ctx, mk(root), state.nav_ratio);
/// bar.restyle(ctx, ComponentConfig{}.with_custom_background(theme::BORDER));
/// ```
std::array<ElementResult, 3>
hsplit_pane(HasUIContext auto &ctx, EntityParent ep_pair, float &ratio,
            ComponentConfig config = ComponentConfig(), float min_ratio = 0.f,
            float max_ratio = 1.f) {
  return detail::split_pane_impl(ctx, ep_pair, FlexDirection::Row, ratio,
                                 std::move(config), min_ratio, max_ratio);
}

/// vsplit's stacked counterpart: `ratio` is the top region's share of the
/// container height. Returns {top, divider, bottom}.
std::array<ElementResult, 3>
vsplit_pane(HasUIContext auto &ctx, EntityParent ep_pair, float &ratio,
            ComponentConfig config = ComponentConfig(), float min_ratio = 0.f,
            float max_ratio = 1.f) {
  return detail::split_pane_impl(ctx, ep_pair, FlexDirection::Column, ratio,
                                 std::move(config), min_ratio, max_ratio);
}


/// Invisible flexible spacer — expands to fill remaining space in a flex
/// container.  Useful for pushing siblings apart (e.g. a label on the left
/// and a control flush-right).
///
/// Default size: expand() x expand() — grows along the parent's flex axis.
/// Override with .with_size() for fixed or proportional spacers.
///
/// Usage:
/// ```cpp
/// auto row = hstack(ctx, mk(parent));
/// button(ctx, mk(row.ent()), ComponentConfig{}.with_label("Left"));
/// spacer(ctx, mk(row.ent()));            // pushes "Right" to the far edge
/// button(ctx, mk(row.ent()), ComponentConfig{}.with_label("Right"));
///
/// // Fixed-width gap
/// spacer(ctx, mk(row.ent()),
///     ComponentConfig{}.with_size(ComponentSize{pixels(16), pixels(1)}));
/// ```
ElementResult spacer(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  if (config.size.is_default) {
    config.with_size(ComponentSize{expand(), expand()});
  }
  config.with_color_usage(Theme::Usage::None).with_skip_tabbing(true);
  return div(ctx, ep_pair, config);
}


/// A tray is a container that acts as a single tab stop.
/// Arrow keys move a selection highlight among its focusable children;
/// Enter/WidgetPress activates the selected child.
///
/// Usage:
/// ```cpp
/// auto t = tray(ctx, mk(parent),
///     ComponentConfig{}.with_size(ComponentSize{percent(1.0f), children()}));
/// button(ctx, mk(t.ent()), ComponentConfig{}.with_label("A"));
/// button(ctx, mk(t.ent()), ComponentConfig{}.with_label("B"));
/// button(ctx, mk(t.ent()), ComponentConfig{}.with_label("C"));
/// ```
ElementResult tray(HasUIContext auto &ctx, EntityParent ep_pair,
                   ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  init_component(ctx, ep_pair, config, ComponentType::Tray, false, "tray");

  // Tray root is a tab stop
  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});
  // Add tray state
  entity.addComponentIfMissing<HasTray>();
  // The tray owns its own arrow keys: otherwise process_tabbing consumes
  // WidgetDown nine systems before HandleTrayNavigation looks for it, and a
  // Column tray -- every dropdown's option list -- cannot be navigated at all.
  entity.addComponentIfMissing<ConsumesDirectionalInput>();

  return ElementResult{false, entity};
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

    init_component(ctx, ep_pair, config, ComponentType::Separator, false,
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
  init_component(ctx, ep_pair, config, ComponentType::Separator, false,
                 "separator");

  return {false, entity};
}


template <typename Container>
ElementResult icon_row(HasUIContext auto &ctx, EntityParent ep_pair,
                       afterhours::texture_manager::Texture spritesheet,
                       const Container &frames, float scale = 1.f,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto row = hstack(ctx, ep_pair,
                    ComponentConfig::inherit_from(config, "icon_row")
                        .with_size(config.size)
                        .with_margin(config.margin)
                        .with_padding(config.padding)
                        .with_debug_name(config.debug_name.empty()
                                             ? "icon_row"
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


} // namespace imm

} // namespace ui

} // namespace afterhours
