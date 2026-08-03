#pragma once

// Menus: one anchored list, three entry points. dropdown_menu opens under a
// trigger button, context_menu opens at a point, popover anchors caller-drawn
// content. All share MenuItem and overlay::place, so flipping and dismissal
// behave the same everywhere.

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

namespace detail {
// Menus close when focus leaves them, which is also how a click outside is
// noticed -- the same rule dropdown already relies on.
template <typename Ctx>
bool menu_lost_focus(Ctx &ctx, EntityID list_id, bool was_open) {
  return was_open && !ctx.has_focus(list_id);
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
  if (!open || items.empty())
    return kNoMenuSelection;

  // The menu's own entity is a zero-size holder: the list hangs off it and is
  // absolutely positioned, but it still needs a UIComponent to be a parent.
  ComponentConfig root_config =
      ComponentConfig::inherit_from(config, "menu_root")
          .with_size(ComponentSize{pixels(0.f), pixels(0.f)})
          .with_skip_tabbing(true);
  init_component(ctx, ep_pair, root_config, ComponentType::Div, false,
                 "menu_root");

  const float item_h = config.size.y_axis.value > 0.f
                           ? config.size.y_axis.value
                           : default_component_size.y;
  const float width =
      config.size.x_axis.value > 0.f ? config.size.x_axis.value : anchor.width;

  float total_h = 0.f;
  for (const auto &it : items)
    total_h += it.separator ? item_h * 0.25f : item_h;

  const auto placed =
      overlay::place(anchor, width, total_h, ctx.screen_width,
                     ctx.screen_height, preferred);

  // with_absolute_position is PARENT-relative, and `anchor` is in screen
  // space, so convert. Passing screen coords straight through adds the
  // parent's own offset a second time.
  const RectangleType parent_rect = parent.template get<UIComponent>().rect();
  const float local_x = placed.x - parent_rect.x;
  const float local_y = placed.y - parent_rect.y;

  auto list = tray(ctx, mk(entity),
                   ComponentConfig::inherit_from(config, "menu_list")
                       .with_size(ComponentSize{pixels(width), children(item_h)})
                       .with_flex_direction(FlexDirection::Column)
                       .with_no_wrap()
                       .with_absolute_position(local_x, local_y)
                       .with_render_layer(config.render_layer + 1));

  int chosen = kNoMenuSelection;
  int index = 0;
  for (const auto &item : items) {
    if (item.separator) {
      div(ctx, mk(list.ent(), index),
          ComponentConfig::inherit_from(config, "menu_separator")
              .with_size(ComponentSize{percent(1.0f), pixels(item_h * 0.25f)})
              .with_skip_tabbing(true));
      index++;
      continue;
    }

    auto row = ComponentConfig::inherit_from(
                   config, fmt::format("menu_item_{}", index))
                   .with_size(ComponentSize{percent(1.0f), pixels(item_h)})
                   .with_label(item.label)
                   .with_disabled(item.disabled);
    auto item_el = button(ctx, mk(list.ent(), index), row);
    if (item_el && !item.disabled)
      chosen = index;

    // Pinned inside the item rather than given its own row, or it lays out as
    // a sibling and lands under the menu.
    if (!item.shortcut.empty()) {
      const float sc_w = width * 0.45f;
      div(ctx, mk(item_el.ent(), 0),
          ComponentConfig::inherit_from(config, "menu_shortcut")
              .with_size(ComponentSize{pixels(sc_w), pixels(item_h)})
              .with_absolute_position(width - sc_w - item_h * 0.3f, 0.f)
              .with_label(item.shortcut)
              .with_alignment(TextAlignment::Right)
              .with_disabled(true)
              .with_skip_tabbing(true)
              .with_render_layer(config.render_layer + 2));
    }
    index++;
  }

  if (chosen != kNoMenuSelection)
    open = false;

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

  if (button(ctx, mk(entity, 0),
             ComponentConfig::inherit_from(config, "dropdown_menu_trigger")
                 .with_size(ComponentSize{percent(1.0f), config.size.y_axis})
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

} // namespace imm
} // namespace ui
} // namespace afterhours
