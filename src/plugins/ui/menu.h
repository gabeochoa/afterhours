#pragma once

// Menus: one anchored list, three entry points. dropdown_menu opens under a
// trigger button, context_menu opens at a point, popover anchors caller-drawn
// content. All share MenuItem and overlay::place, so flipping and dismissal
// behave the same everywhere.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "imm_components.h"
#include "overlay.h"

namespace afterhours {
namespace ui {
namespace imm {

struct MenuItem {
  std::string label;
  std::string shortcut;      // right-aligned hint, e.g. "Cmd+S"
  bool separator = false;    // a rule; not selectable, ignores label
  bool disabled = false;

  static MenuItem sep() {
    MenuItem m;
    m.separator = true;
    return m;
  }
};

// Result of a menu: index of the item chosen this frame, or -1.
inline constexpr int kNoMenuSelection = -1;

// Tracks the opening edge, panel, and focus to restore on dismissal.
struct HasMenuState : BaseComponent {
  bool was_open_last_frame = false;
  EntityID panel = -1;
  EntityID restore_focus = -1;
};

namespace detail {
template <typename Ctx>
bool dismiss_menu(Ctx &ctx, HasMenuState &state, bool open) {
  if (!state.was_open_last_frame) return !open;
  const bool inside = ctx.focus_in_subtree(state.panel);
  const bool outside_press = ctx.mouse.just_pressed &&
      !is_point_inside_entity_tree(state.panel, ctx.mouse.pos);
  using Action = std::remove_cvref_t<decltype(ctx.last_action)>;
  bool escape = false;
  if constexpr (magic_enum::enum_contains<Action>("MenuBack"))
    escape = inside && ctx.pressed(Action::MenuBack);
  if (open && inside && !escape && !outside_press) return false;
  if (inside) ctx.set_focus(state.restore_focus);
  state.was_open_last_frame = false;
  auto panel = UICollectionHolder::getEntityForID(state.panel);
  if (panel.valid()) panel.asE().template addComponentIfMissing<ShouldHide>();
  return true;
}

// Focus anywhere in a subtree. A menu can use exact-id focus because focusing
// an item means choosing it, but a popover holds arbitrary controls and must
// survive focus landing on one of them.
//
// Walks UP from the focused element, not down from `id`. The tree is cleared
// and rebuilt every frame and a popover runs this check before its caller has
// re-added any content, so walking down would always see an empty subtree.
// ClearUIComponentChildren empties `children` but leaves `parent` intact.
// The upward walk lives on UIContext so the hover query shares it.
template <typename Ctx> bool focus_within(Ctx &ctx, EntityID id) {
  return ctx.focus_in_subtree(id);
}
} // namespace detail

// The shared list. `anchor` is what it opens against, in screen space.
// Returns the clicked index, or kNoMenuSelection.
template <typename Container>
int menu_list(HasUIContext auto &ctx, EntityParent ep_pair,
              const Container &items, const RectangleType &anchor, bool &open,
              overlay::Placement preferred = overlay::Placement::Below,
              ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  auto &state = entity.template addComponentIfMissing<HasMenuState>();
  if (!open || items.empty()) {
    detail::dismiss_menu(ctx, state, false);
    open = false;
    state.was_open_last_frame = false;
    return kNoMenuSelection;
  }

  // The menu's own entity is a zero-size holder: the list hangs off it and is
  // absolutely positioned, but it still needs a UIComponent to be a parent.
  ComponentConfig root_config =
      ComponentConfig::inherit_from(config, "menu_root")
          .with_size(ComponentSize{pixels(0.f), pixels(0.f)})
          .without_border()
          .with_skip_tabbing(true);
  init_component(ctx, ep_pair, root_config, ComponentType::Div, false,
                 "menu_root");

  const float item_h = config.size.y_axis.value > 0.f
                           ? config.size.y_axis.value
                           : default_component_size.y;
  const float width =
      config.size.x_axis.value > 0.f ? config.size.x_axis.value : anchor.width;

  float total_h = 0.f;
  size_t longest_shortcut = 0;
  for (const auto &it : items) {
    total_h += it.separator ? item_h * 0.25f : item_h;
    longest_shortcut = std::max(longest_shortcut, it.shortcut.size());
  }

  // Gutter for the shortcuts. No font is reachable here, so a glyph is
  // approximated at half the row height -- over-wide is harmless since the
  // shortcut is right-aligned inside it. Half the menu is the ceiling: past
  // that the label has nowhere to go and truncating the shortcut is better.
  const float shortcut_pad = item_h * 0.25f;
  const float gutter =
      longest_shortcut == 0
          ? 0.f
          : std::min(width * 0.5f, static_cast<float>(longest_shortcut) *
                                           item_h * 0.3f +
                                       shortcut_pad * 2.f);

  const auto placed =
      overlay::place(anchor, width, total_h, ctx.screen_width,
                     ctx.screen_height, preferred);

  // with_absolute_position is relative to the IMMEDIATE parent, which for the
  // list is menu_root -- not menu_root's parent. Subtracting the latter leaves
  // menu_root's own offset in, and inside a dropdown holder that offset is the
  // trigger's height, so every menu lands one trigger too far along.
  const RectangleType root_rect = entity.template get<UIComponent>().rect();
  const float local_x = placed.x - root_rect.x;
  const float local_y = placed.y - root_rect.y;

  // The list is the panel: it owns the background, so rows sit flush and a
  // separator reads as a band on a continuous surface. Square, like everything
  // else in the menu -- a rounded panel shows through the last row whenever
  // that row is disabled, since disabled backgrounds are translucent.
  // A border draws inside the panel rect and layout does not inset children,
  // so without this the rows paint over three of its four edges.
  const float bw = config.border_config.has_value()
                       ? config.border_config->top.thickness.value
                       : 0.f;

  auto list = tray(ctx, mk(entity),
                   ComponentConfig::inherit_from(config, "menu_list")
                       .with_background(Theme::Usage::Surface)
                       .disable_rounded_corners()
                       .with_padding(Padding::all(pixels(bw)))
                       .with_size(ComponentSize{pixels(width), children(item_h)})
                       .with_flex_direction(FlexDirection::Column)
                       .with_no_wrap()
                       .with_absolute_position(local_x, local_y)
                       .with_render_layer(config.render_layer + 1));

  int chosen = kNoMenuSelection;
  int index = 0;
  float row_y = 0.f; // running offset of the current row inside the list
  for (const auto &item : items) {
    if (item.separator) {
      // Transparent band: the list's surface shows through it, which is the
      // divider. A drawn rule would need to fit a band this thin, and the
      // theme's separator thickness is sized for full rows.
      div(ctx, mk(list.ent(), index),
          ComponentConfig::inherit_from(config, "menu_separator")
              .with_size(ComponentSize{percent(1.0f), pixels(item_h * 0.25f)})
              .disable_rounded_corners()
              .without_border()
              .with_transparent_bg()
              .with_skip_tabbing(true));
      row_y += item_h * 0.25f;
      index++;
      continue;
    }

    // Square rows: rounding each one carves notches out of the edge it shares
    // with its neighbour, which reads as a gap between them.
    auto row = ComponentConfig::inherit_from(
                   config, fmt::format("menu_item_{}", index))
                   .with_size(ComponentSize{percent(1.0f), pixels(item_h)})
                   .disable_rounded_corners()
                   .without_border()
                   .with_label(item.label)
                   .with_disabled(item.disabled);
    // Labels default to centred, which walks straight into the gutter.
    if (gutter > 0.f)
      row.with_alignment(TextAlignment::Left);

    // A disabled row still has to read as a row. Its own background is muted
    // to near-transparent, which left it 5/255 from the panel and looking
    // like a wide separator, so the fill comes from an underlay instead and
    // only the label dims.
    if (item.disabled) {
      div(ctx, mk(list.ent(), 20000 + index),
          ComponentConfig::inherit_from(config, "menu_row_fill")
              .with_size(ComponentSize{pixels(width - bw * 2.f),
                                       pixels(item_h)})
              .with_absolute_position(0.f, row_y)
              .disable_rounded_corners()
              .without_border()
              .with_custom_background(colors::lighten(
                  ctx.theme.from_usage(Theme::Usage::Surface), 0.06f))
              .with_skip_tabbing(true)
              .with_render_layer(config.render_layer + 1));
      row.with_custom_background(colors::transparent());
    }
    auto item_el = button(ctx, mk(list.ent(), index), row);
    if (item_el && !item.disabled)
      chosen = index;

    // Positioned against the LIST, not the item: inside the item it lands in
    // the item's content box (after padding) and spills past the menu edge.
    //
    // Held off the right edge by the same pad the gutter reserves. Ending the
    // box exactly at `width` put right-aligned text hard against the panel
    // boundary and the last glyph was clipped in half -- there is no bearing
    // left for it once the box runs out.
    if (!item.shortcut.empty()) {
      div(ctx, mk(list.ent(), 10000 + index),
          ComponentConfig::inherit_from(config, "menu_shortcut")
              .with_size(ComponentSize{pixels(gutter - shortcut_pad),
                                       pixels(item_h)})
              .without_border()
              .with_absolute_position(width - gutter, row_y)
              .with_label(item.shortcut)
              .with_alignment(TextAlignment::Right)
              .with_disabled(true)
              .with_skip_tabbing(true)
              .with_render_layer(config.render_layer + 2));
    }
    row_y += item_h;
    index++;
  }

  state.panel = list.ent().id;
  if (!state.was_open_last_frame) {
    state.restore_focus = ctx.focus_id;
    ctx.set_focus(state.panel);
  }
  if (chosen != kNoMenuSelection) open = false;
  if (detail::dismiss_menu(ctx, state, open)) open = false;
  state.was_open_last_frame = open;

  return chosen;
}

// Trigger button plus a menu beneath it.
template <typename Container>
int dropdown_menu(HasUIContext auto &ctx, EntityParent ep_pair,
                  const std::string &label, const Container &items, bool &open,
                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  // Container for the trigger + the menu. Uses the caller's config rather than
  // inherit_from, which drops position -- the holder is what gets placed, and
  // the menu anchors to its rect.
  ComponentConfig holder = config;
  holder.with_debug_name("dropdown_menu");
  init_component(ctx, ep_pair, holder, ComponentType::Div, false,
                 "dropdown_menu");

  // Square too: an open menu butts straight up against the trigger, and a
  // rounded trigger notches that seam.
  if (button(ctx, mk(entity, 0),
             ComponentConfig::inherit_from(config, "dropdown_menu_trigger")
                 .with_size(ComponentSize{percent(1.0f), config.size.y_axis})
                 .disable_rounded_corners()
                 .with_label(label)))
    open = !open;

  const RectangleType anchor = entity.template get<UIComponent>().rect();
  return menu_list(ctx, mk(entity, 1), items, anchor, open,
                   overlay::Placement::Below, config);
}

// A menu at a point, for right-click. `at` is in screen space.
template <typename Container>
int context_menu(HasUIContext auto &ctx, EntityParent ep_pair,
                 const Container &items, const Vector2Type &at, bool &open,
                 ComponentConfig config = ComponentConfig()) {
  const RectangleType anchor{at.x, at.y, 0.f, 0.f};
  return menu_list(ctx, ep_pair, items, anchor, open,
                   overlay::Placement::Below, config);
}

// An anchored panel whose contents the caller draws. Same placement and
// dismissal as a menu; the difference is what closes it -- see focus_within.
//
//   auto pop = popover(ctx, mk(entity), anchor, open, Placement::Below,
//                      ComponentConfig{}.with_size({pixels(220),
//                                                   pixels(140)}));
//   if (pop)
//     button(ctx, mk(pop.ent(), 0), ComponentConfig{}.with_label("Apply"));
//
// Returns the panel, falsy when closed, so the body only runs when open.
inline ElementResult
popover(HasUIContext auto &ctx, EntityParent ep_pair,
        const RectangleType &anchor, bool &open,
        overlay::Placement preferred = overlay::Placement::Below,
        ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);
  auto &state = entity.template addComponentIfMissing<HasMenuState>();
  if (detail::dismiss_menu(ctx, state, open)) {
    open = false;
    state.was_open_last_frame = false;
    return ElementResult{false, entity};
  }

  ComponentConfig root_config =
      ComponentConfig::inherit_from(config, "popover_root")
          .with_size(ComponentSize{pixels(0.f), pixels(0.f)})
          .without_border()
          .with_skip_tabbing(true);
  init_component(ctx, ep_pair, root_config, ComponentType::Div, false,
                 "popover_root");

  const float width =
      config.size.x_axis.value > 0.f ? config.size.x_axis.value : anchor.width;
  const float height = config.size.y_axis.value > 0.f
                           ? config.size.y_axis.value
                           : default_component_size.y;

  const auto placed = overlay::place(anchor, width, height, ctx.screen_width,
                                     ctx.screen_height, preferred);

  // Parent-relative, against popover_root itself -- see the same conversion in
  // menu_list for why its parent is the wrong thing to subtract.
  const RectangleType root_rect = entity.template get<UIComponent>().rect();

  auto panel =
      tray(ctx, mk(entity),
           ComponentConfig::inherit_from(config, "popover_panel")
               .with_background(Theme::Usage::Surface)
               .with_size(ComponentSize{pixels(width), pixels(height)})
               .with_flex_direction(FlexDirection::Column)
               .with_absolute_position(placed.x - root_rect.x,
                                       placed.y - root_rect.y)
               .with_render_layer(config.render_layer + 1));

  state.panel = panel.ent().id;
  if (!state.was_open_last_frame) {
    state.restore_focus = ctx.focus_id;
    ctx.set_focus(state.panel);
  }
  state.was_open_last_frame = true;

  return ElementResult{true, panel.ent()};
}

} // namespace imm
} // namespace ui
} // namespace afterhours
