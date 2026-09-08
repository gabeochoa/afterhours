#pragma once

// Hover text. Nothing in the library offered it, so every consumer that wanted
// a tooltip hand-rolled a popover and positioned it themselves.
//
// Placement goes through overlay::place, the same flipping and clamping a
// dropdown uses, so a tooltip on an element near an edge turns to the other
// side instead of going off screen.

#include <string>

#include "../../developer.h"
#include "components.h"
#include "context.h"
#include "overlay.h"

namespace afterhours {
namespace ui {

struct HasTooltip : BaseComponent {
  std::string text;
  // Hovering is not asking. A tooltip that appears the instant the cursor
  // crosses something flashes at anyone moving across the screen.
  float delay = 0.5f;
  overlay::Placement placement = overlay::Placement::Below;

  HasTooltip() = default;
  HasTooltip(std::string t, float d, overlay::Placement p)
      : text(std::move(t)), delay(d), placement(p) {}
};

// Which tooltip is up, and how long its element has been hovered. Singleton:
// only one shows at a time, because the cursor is only in one place.
struct TooltipState : BaseComponent {
  EntityID hovered = -1;
  float elapsed = 0.f;
  // Set once the delay is met; what the renderer looks for.
  EntityID showing = -1;
  RectangleType anchor{};
  std::string text;

  bool is_showing() const { return showing != -1 && !text.empty(); }
};

namespace tooltip_detail {

// Rough, but it only decides a box size and the renderer clips to it anyway.
inline float measure_width(const std::string &text, float font_size) {
  return static_cast<float>(text.size()) * font_size * 0.5f;
}

} // namespace tooltip_detail

template <typename InputAction>
struct UpdateTooltips : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float dt) override {
    auto *state = EntityHelper::get_singleton_cmp<TooltipState>();
    if (state == nullptr)
      return;

    // Find the hovered element that actually wants a tooltip. The hot element
    // is often a child that has none, so walk up: hovering a button's label
    // should still count as hovering the button.
    EntityID owner = -1;
    RectangleType anchor{};
    std::string text;
    float delay = 0.5f;

    for (int id = context.hot_id; id >= 0;) {
      OptEntity oe = UICollectionHolder::getEntityForID(id);
      if (!oe.valid())
        break;
      Entity &e = oe.asE();
      if (e.has<HasTooltip>() && e.has<UIComponent>()) {
        owner = id;
        anchor = e.get<UIComponent>().rect();
        text = e.get<HasTooltip>().text;
        delay = e.get<HasTooltip>().delay;
        break;
      }
      if (!e.has<UIComponent>())
        break;
      id = e.get<UIComponent>().parent;
    }

    if (owner == -1) {
      // Field by field: BaseComponent is not assignable.
      state->hovered = -1;
      state->elapsed = 0.f;
      state->showing = -1;
      state->text.clear();
      return;
    }

    // Moving to a different element restarts the wait rather than inheriting
    // the last one's, so dragging across a toolbar does not pop every button.
    if (state->hovered != owner) {
      state->hovered = owner;
      state->elapsed = 0.f;
      state->showing = -1;
      state->text.clear();
      return;
    }

    state->elapsed += dt;
    if (state->elapsed < delay)
      return;

    state->showing = owner;
    state->anchor = anchor;
    state->text = text;
  }
};

// Drawn last and outside the layout tree, so it sits over whatever it
// describes rather than being clipped by it.
template <typename InputAction>
struct RenderTooltip : System<UIContext<InputAction>> {
  virtual void for_each_with(Entity &, UIContext<InputAction> &context,
                             float) override {
    const auto *state = EntityHelper::get_singleton_cmp<TooltipState>();
    if (state == nullptr || !state->is_showing())
      return;

    const float font_size = 14.f;
    const float pad_x = 8.f, pad_y = 5.f;
    const float w = tooltip_detail::measure_width(state->text, font_size) +
                    pad_x * 2.f;
    const float h = font_size + pad_y * 2.f;

    const auto placed = overlay::place(state->anchor, w, h,
                                       context.screen_width,
                                       context.screen_height,
                                       overlay::Placement::Below, 4.f);

    const RectangleType box{placed.x, placed.y, w, h};
    draw_rectangle_rounded(box, 0.25f, 6, context.theme.from_usage(
                                              Theme::Usage::Surface, false),
                           std::bitset<4>().set());
    draw_rectangle_rounded_lines(
        box, 0.25f, 6, context.theme.from_usage(Theme::Usage::Accent, false),
        std::bitset<4>().set());
    auto *fm = EntityHelper::get_singleton_cmp<FontManager>();
    if (fm == nullptr)
      return;
    draw_text_ex(fm->get_active_font(), state->text.c_str(),
                 Vector2Type{box.x + pad_x, box.y + pad_y}, font_size, 1.f,
                 context.theme.from_usage(Theme::Usage::Font, false));
  }
};

} // namespace ui
} // namespace afterhours
