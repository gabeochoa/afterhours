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

template <typename Container>
ElementResult dropdown(HasUIContext auto &ctx, EntityParent ep_pair,
                       const Container &options, size_t &option_index,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasDropdownState &dropdownState = init_state<HasDropdownState>(
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

  init_component(ctx, ep_pair, config, ComponentType::Dropdown);

  auto button_corners =
      config.rounded_corners.value_or(ctx.theme.rounded_corners);

  auto config_size = config.size;
  if (config_size.x_axis.dim != Dim::Children)
    config_size.x_axis = percent(1.f);

  bool has_label_child = !label_str.empty();
  if (has_label_child) {
    config_size = config_size.scale_x(0.5f);
    button_corners = RoundedCorners(button_corners).left_sharp();

    auto label = div(
        ctx, mk(entity),
        ComponentConfig::inherit_from(config, "dropdown_label")
            .with_size(config_size)
            .with_label(std::string(label_str))
            .with_color_usage(config.color_usage == Theme::Usage::Default
                ? Theme::Usage::None : config.color_usage)
            .with_rounded_corners(RoundedCorners(button_corners).right_sharp())
            .with_render_layer(config.render_layer + 0));
    label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // Index of the trigger button within the dropdown entity's children
  const size_t trigger_child_index = label_str.empty() ? 0 : 1;

  const auto on_option_click = [&](Entity &, size_t opt) {
    dropdownState.on = false;
    dropdownState.last_option_clicked = opt;
    dropdownState.changed_since = true;

    EntityID id = entity.get<UIComponent>().children[trigger_child_index];
    Entity &trigger = UICollectionHolder::getEntityForIDEnforce(id);
    trigger.get<ui::HasLabel>().label =
        fmt::format("{}{}", std::string(options[opt]),
                    config.dropdown_closed_indicator.value_or(
                        ComponentConfig::DEFAULT_DROPDOWN_CLOSED));
    ctx.set_focus(trigger.id);
  };

  auto current_option = std::string(options[dropdownState.last_option_clicked]);
  auto drop_closed = config.dropdown_closed_indicator.value_or(
      ComponentConfig::DEFAULT_DROPDOWN_CLOSED);
  auto drop_open = config.dropdown_open_indicator.value_or(
      ComponentConfig::DEFAULT_DROPDOWN_OPEN);
  auto drop_arrow_icon = dropdownState.on ? drop_open : drop_closed;
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
  auto trigger_config = ComponentConfig::inherit_from(config, "option 1")
      .with_size(config_size)
      .with_label(main_button_label)
      .with_rounded_corners(button_corners)
      .with_render_layer(config.render_layer);
  if (config.color_usage == Theme::Usage::Default && !config.custom_color) {
    trigger_config.with_color_usage(Theme::Usage::Secondary);
    if (!trigger_config.has_border())
      trigger_config.with_border(ctx.theme.control_border(ctx.theme.secondary), 1.f);
  }
  auto main_btn = button(ctx, mk(entity), trigger_config);
  if (main_btn) {
    dropdownState.on = !dropdownState.on;
  }

  // Mark the label + main dropdown button as a focus cluster, but do not
  // include dropdown items (they should be separately focusable when open).
  entity.template addComponentIfMissing<FocusClusterRoot>();
  // Mark the main dropdown button as part of the cluster
  main_btn.ent().template addComponentIfMissing<InFocusCluster>();

  // When open, show options in a tray for arrow-key navigation
  if (dropdownState.on) {
    // Flip above the trigger when the tray would run off the bottom. Options
    // are uniform, so the height is known before layout.
    const RectangleType anchor = entity.template get<UIComponent>().rect();
    const float row_height = main_btn.cmp().height() > 0.f
        ? main_btn.cmp().height() : config.size.y_axis.value;
    const float available = std::max(anchor.y, ctx.screen_height - anchor.y - anchor.height);
    const float tray_h = std::min(static_cast<float>(options.size()) * row_height,
                                  std::max(1.f, available));
    const auto placed = overlay::place(
        anchor, anchor.width, tray_h, ctx.screen_width, ctx.screen_height,
        overlay::Placement::Below);

    auto options_tray =
        tray(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "dropdown_options_tray")
                 .with_size(ComponentSize{percent(1.0f),
                                          pixels(tray_h)})
                 .with_overflow(Overflow::Scroll, Axis::Y)
                 .with_flex_direction(FlexDirection::Column)
                 .with_no_wrap()
                 .with_absolute_position()
                 .with_translate(pixels(0), pixels(placed.y - anchor.y))
                 .with_render_layer(config.render_layer + 1));

    // Set tray selection to current option only when first opened
    if (!dropdownState.was_open_last_frame &&
        options_tray.ent().template has<HasTray>()) {
      options_tray.ent().template get<HasTray>().selection_index =
          static_cast<int>(dropdownState.last_option_clicked);
    }

    for (size_t i = 0; i < options.size(); ++i) {
      if (button(
              ctx, mk(options_tray.ent(), i),
              ComponentConfig::inherit_from(config,
                                            fmt::format("dropdown_opt_{}", i))
                  .with_size(ComponentSize{percent(1.0f), pixels(row_height)})
                  .with_label(std::string(options[i])))) {
        on_option_click(entity, i);
      }
    }

    // Move focus to tray when first opened
    if (!dropdownState.was_open_last_frame) {
      ctx.set_focus(options_tray.ent().id);
    }

    // Escape or an outside press closes without changing selection
    using IA = typename std::remove_reference_t<decltype(ctx)>::value_type;
    const bool outside_press = dropdownState.was_open_last_frame &&
        ctx.mouse.just_pressed &&
        !is_point_inside_entity_tree(entity.id, ctx.mouse.pos);
    if (ctx.pressed(IA::MenuBack) || outside_press) {
      dropdownState.on = false;
      EntityID trigger_id =
          entity.get<UIComponent>().children[trigger_child_index];
      ctx.set_focus(trigger_id);
    }

    // Close on focus loss (handles Tab-to-close and click-outside-to-close)
    if (dropdownState.was_open_last_frame && dropdownState.on) {
      bool focus_in_dropdown = ctx.has_focus(options_tray.ent().id);
      if (!focus_in_dropdown) {
        dropdownState.on = false;
      }
    }
  }

  dropdownState.was_open_last_frame = dropdownState.on;

  // TODO add a way to set tags directly from a bool
  // Block clicks on elements behind the open dropdown
  if (dropdownState.on) {
    entity.enableTag(UITag::InputExclusivity);
  } else {
    entity.disableTag(UITag::InputExclusivity);
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}


// TODO: Consider making navigation_bar a thin wrapper around
// stepper(num_visible=3)
template <typename Container>
ElementResult navigation_bar(HasUIContext auto &ctx, EntityParent ep_pair,
                             const Container &options, size_t &option_index,
                             ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasNavigationBarState &navState = init_state<HasNavigationBarState>(
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
  config.with_no_wrap(); // Prevent arrow buttons from wrapping to new line

  // Prevent the parent navigation bar from getting a background color
  config.with_color_usage(Theme::Usage::None);
  init_component(ctx, ep_pair, config, ComponentType::NavigationBar, false,
                 "navigation_bar");

  bool clicked = false;
  size_t new_index = navState.current_index();

  // Use slightly under 100% total to avoid floating point precision issues
  // in layout overflow detection (20% + 59% + 20% = 99%)
  constexpr float arrow_ratio = 0.20f;
  constexpr float label_ratio = 0.59f;

  auto arrow_size = ComponentSize{percent(arrow_ratio), config.size.y_axis};

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "left_arrow")
                 .with_size(arrow_size)
                 .with_label("<")
                 .with_font(UIComponent::SYMBOL_FONT, config.font_size)
                 .with_rounded_corners(RoundedCorners().left_round())
                 .with_margin(Margin{}))) {
    clicked = true;
    new_index = detail::prev_index(navState.current_index(), options.size());
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
                 .with_font(UIComponent::SYMBOL_FONT, config.font_size)
                 .with_rounded_corners(RoundedCorners().right_round())
                 .with_margin(Margin{}))) {
    clicked = true;
    new_index = detail::next_index(navState.current_index(), options.size());
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
  init_component(ctx, ep_pair, config, ComponentType::TabContainer, false,
                 "tab_container");

  bool changed = false;

  size_t i = 0;
  for (const auto &label : tab_labels) {
    bool is_active = (i == active_tab);

    // Active tab: surface color with bold text
    // Inactive tab: slightly darkened background with muted text
    Color tab_bg =
        is_active ? ctx.theme.surface
                  : afterhours::colors::darken(ctx.theme.background, 0.92f);
    Color tab_text = is_active ? ctx.theme.font : ctx.theme.font_muted;

    // Active tab: bold accent underline (4px)
    // Inactive tab: subtle muted line to separate from content
    Color underline_color =
        is_active ? ctx.theme.accent
                  : afterhours::colors::darken(ctx.theme.background, 0.80f);
    float underline_h = is_active ? 4.0f : 1.0f;

    // Tabs share the bar via expand() (even distribution), but each tab gets a
    // min-width equal to its own label so long labels never ellipsize when the
    // bar has room — short-label bars still look evenly split, long-label bars
    // grow tabs to fit instead of truncating.
    auto tab_config =
        ComponentConfig::inherit_from(config, fmt::format("tab_{}", i))
            .with_size(ComponentSize{expand(), config.size.y_axis})
            .with_label(std::string(label))
            .with_custom_background(tab_bg)
            .with_custom_text_color(tab_text)
            .with_align_items(AlignItems::Center)
            .with_justify_content(JustifyContent::Center)
            .with_text_overflow(TextOverflow::Ellipsis)
            .with_border_bottom(underline_color, pixels(underline_h));

    auto tab = button(ctx, mk(entity, i), tab_config);
    // Content-fit floor: never shrink a tab below its label width (+ padding).
    tab.ent().template get<UIComponent>().set_min_width(
        Size{Dim::Text, 0.f, 1.f});
    if (tab) {
      active_tab = i;
      changed = true;
    }

    ++i;
  }

  return ElementResult{changed, entity, static_cast<int>(active_tab)};
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
ElementResult decorative_frame(
    HasUIContext auto &ctx, EntityParent ep_pair,
    ComponentConfig config = ComponentConfig(),
    DecorativeFrameStyle style = DecorativeFrameStyle::KraftPaper) {
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
                                  ? config.border_config->uniform_thickness()
                                  : h720(12.0f);
  float frame_thickness =
      resolve_to_pixels(frame_thickness_size, screen_height);

  // Determine colors
  // Frame color: from border config, or derive from theme
  // Background color: from custom_color or theme
  Color frame_color = config.border_config.has_value()
                          ? config.border_config->uniform_color()
                          : ctx.theme.secondary;
  Color bg_color = config.custom_color.value_or(ctx.theme.surface);

  // Initialize main container (no background - children will draw it)
  config.with_color_usage(Theme::Usage::None);
  init_component(ctx, ep_pair, config, ComponentType::DecorativeFrame, false,
                 "decorative_frame");

  // Get computed size for positioning edge elements (corners, highlights)
  // Note: On first frame, computed values may be 0 - edge elements skip
  // rendering
  UIComponent &cmp = entity.template get<UIComponent>();
  float w = cmp.computed[Axis::X];
  float h = cmp.computed[Axis::Y];
  bool has_computed_size = w > 0 && h > 0;

  // Inner layers fill the decorative_frame using computed pixel values
  // This avoids percentage compounding issues across nested elements
  float fill_w = w > 0 ? w : 100.f;
  float fill_h = h > 0 ? h : 100.f;
  ComponentSize fill_size{pixels(fill_w), pixels(fill_h)};

  // An inset layer has to shrink by twice the inset, not just shift by it, or
  // it overhangs the bottom-right instead of nesting inside the frame.
  auto inset_fill = [&](float px) {
    return ComponentSize{pixels(std::max(0.f, fill_w - 2.f * px)),
                         pixels(std::max(0.f, fill_h - 2.f * px))};
  };

  if (style == DecorativeFrameStyle::KraftPaper) {
    // Kraft paper style: multiple layered borders with corner accents

    // Outer dark border (fills the whole frame area)
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(fill_size)
            .with_absolute_position()
            .with_custom_background(frame_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_outer"));

    // Inner lighter border (creates depth)
    Color lighter_frame = colors::lighten(frame_color, 0.1f);

    Size inset1 = pixels(frame_thickness * 0.3f);
    div(ctx, mk(entity, 1),
        ComponentConfig{}
            .with_size(inset_fill(frame_thickness * 0.3f))
            .with_absolute_position()
            .with_translate(inset1, inset1)
            .with_custom_background(lighter_frame)
            .with_skip_tabbing(true)
            .with_debug_name("frame_inner1"));

    // Main background
    Size inset2 = pixels(frame_thickness);
    div(ctx, mk(entity, 2),
        ComponentConfig{}
            .with_size(inset_fill(frame_thickness))
            .with_absolute_position()
            .with_translate(inset2, inset2)
            .with_custom_background(bg_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_bg"));

    // Corner accents for "hand-made" feel (only render when size is computed)
    if (has_computed_size) {
      Size corner_size = h720(8.0f);
      // Sit the accents on the frame band, clear of the rounded corner arc.
      Size corner_offset = pixels(frame_thickness);
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
              .with_translate(w - corner_size_px - corner_offset_px,
                              corner_offset_px)
              .with_custom_background(corner_color)
              .with_skip_tabbing(true)
              .with_debug_name("corner_tr"));

      // Bottom-left corner
      div(ctx, mk(entity, 5),
          ComponentConfig{}
              .with_size(ComponentSize{corner_size, corner_size})
              .with_absolute_position()
              .with_translate(corner_offset_px,
                              h - corner_size_px - corner_offset_px)
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
            .with_absolute_position()
            .with_custom_background(bg_color)
            .with_border(frame_color, frame_thickness)
            .with_skip_tabbing(true)
            .with_debug_name("frame_simple"));

  } else if (style == DecorativeFrameStyle::Inset) {
    // Inset style: sunken effect with shadow
    Color shadow_color =
        colors::opacity_pct(colors::darken(frame_color, 0.8f), 0.6f);
    Color highlight_color = colors::lighten(frame_color, 0.2f);

    // Outer frame
    div(ctx, mk(entity, 0),
        ComponentConfig{}
            .with_size(fill_size)
            .with_absolute_position()
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
            .with_size(inset_fill(frame_thickness))
            .with_absolute_position()
            .with_translate(inset, inset)
            .with_custom_background(bg_color)
            .with_skip_tabbing(true)
            .with_debug_name("frame_bg"));
  }

  return ElementResult{false, entity};
}


} // namespace imm

} // namespace ui

} // namespace afterhours
