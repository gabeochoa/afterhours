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
#include "styling_defaults.h"
#include "text_selection.h"

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
  overlay::Placement placement = overlay::Placement::Below;

  bool is_showing() const { return showing != -1 && !text.empty(); }
};

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
    auto placement = overlay::Placement::Below;

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
        placement = e.get<HasTooltip>().placement;
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
    state->placement = placement;
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

    auto *fonts = EntityHelper::get_singleton_cmp<FontManager>();
    if (fonts == nullptr || context.screen_width <= 0 || context.screen_height <= 0)
      return;

    const auto &defaults = imm::UIStylingDefaults::get();
    const auto font = fonts->get_font(defaults.resolved_font_name());
    const auto mode = context.scaling_mode.value_or(defaults.scaling_mode);
    const float font_size = resolve_to_pixels(defaults.default_font_size,
        context.screen_height, mode, context.theme.ui_scale);
    if (font_size <= 0.f)
      return;

    const float scale = mode == ScalingMode::Adaptive
                            ? context.theme.ui_scale
                            : context.screen_height / 720.f;
    const float padding = 8.f * scale;
    const float max_width = std::min(360.f * scale,
                                    context.screen_width - padding * 2.f);
    if (max_width <= padding * 2.f)
      return;

    const auto measure = [&](const std::string &text) {
      return measure_text(font, text.c_str(), font_size, 1.f).x;
    };
    const auto lines = detail::wrap_text_to_width(
        state->text, max_width - padding * 2.f, measure);
    float width = 0.f;
    for (const auto &line : lines)
      width = std::max(width, measure(line));
    const float line_height = std::max(font_size,
        measure_text(font, "Ag", font_size, 1.f).y) * 1.5f;
    const float w = std::min(max_width, width + padding * 2.f);
    const float h = std::min(context.screen_height,
        line_height * static_cast<float>(lines.size()) + padding * 2.f);
    const auto placed = overlay::place(state->anchor, w, h,
        context.screen_width, context.screen_height, state->placement, 4.f * scale);

    const RectangleType box{placed.x, placed.y, w, h};
    const float roundness = resolve_roundness(context.theme.corner_radius,
                                             context.theme.roundness, box);
    const auto background = context.theme.raised_surface();
    draw_rectangle_rounded(box, roundness, context.theme.segments, background,
                           context.theme.rounded_corners);
    draw_rectangle_rounded_lines(box, roundness, context.theme.segments,
        context.theme.subtle_border(), context.theme.rounded_corners);
    begin_scissor_mode(static_cast<int>(box.x + padding),
                       static_cast<int>(box.y + padding),
                       static_cast<int>(std::max(0.f, w - padding * 2.f)),
                       static_cast<int>(std::max(0.f, h - padding * 2.f)));
    float y = box.y + padding;
    const auto foreground = colors::auto_text_color(
        background, context.theme.font, context.theme.darkfont);
    for (const auto &line : lines) {
      draw_text_ex(font, line.c_str(), Vector2Type{box.x + padding, y},
                   font_size, 1.f, foreground);
      y += line_height;
    }
    end_scissor_mode();
  }
};

} // namespace ui
} // namespace afterhours
