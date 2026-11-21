#pragma once

#include <functional>
#include <string>

#include "../../polyline.h"
#include "component_init.h"
#include "element_result.h"
#include "entity_management.h"
#include "imm_components.h"

namespace afterhours {

namespace ui {

namespace imm {

struct HasDisclosureState : BaseComponent {
  bool expanded = false;
  bool changed_since = false;
  HasDisclosureState(bool expanded_ = false) : expanded(expanded_) {}
};

struct DisclosureConfig {
  bool default_expanded = false;
  float header_height = 44.f;
  float chevron_size = 14.f;
};

// A header row (drawn chevron + label) that shows or hides the body built by
// `body`. The body callback runs only while expanded and receives the body
// container entity to add children to with mk(). Expanded state lives on the
// widget entity (HasDisclosureState), so it survives across frames; the
// result is true only on the frame the header was activated.
template <typename BodyFn>
ElementResult disclosure(HasUIContext auto &ctx, EntityParent ep_pair,
                         const std::string &label, BodyFn &&body,
                         DisclosureConfig disclosure_config = {},
                         ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (config.size.is_default) {
    config.with_size(ComponentSize{percent(1.f), children()});
  }
  config.with_flex_direction(FlexDirection::Column);
  init_component(ctx, ep_pair, config, ComponentType::Div, false, "disclosure");

  auto &state = init_state<HasDisclosureState>(
      entity, [](HasDisclosureState &) {}, disclosure_config.default_expanded);

  const bool expanded = state.expanded;
  const Color chevron_color = ctx.theme.font;

  auto header =
      button(ctx, mk(entity, 0),
             ComponentConfig::inherit_from(config, "disclosure_header")
                 .with_size(ComponentSize{percent(1.f),
                                          pixels(disclosure_config.header_height)})
                 .with_flex_direction(FlexDirection::Row)
                 .with_align_items(AlignItems::Center)
                 // Small padding so the label child is not clipped to the
                 // button's default padded content box.
                 .with_padding(Padding::all(pixels(2)))
                 .with_label(""));
  bool changed = false;
  if (header && !config.disabled) {
    state.expanded = !state.expanded;
    state.changed_since = true;
    changed = true;
  }

  // The chevron is drawn, not a text glyph, so it renders in any font.
  div(ctx, mk(header.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(disclosure_config.chevron_size),
                                   pixels(disclosure_config.chevron_size)})
          .with_skip_tabbing(true)
          .with_debug_name("disclosure_chevron")
          .with_on_draw_fg([expanded, chevron_color](RectangleType r) {
            const float inset = 3.f;
            polyline::draw_chevron_in_rect(
                RectangleType{r.x + inset, r.y + inset, r.width - 2.f * inset,
                              r.height - 2.f * inset},
                expanded ? polyline::ChevronDirection::Down
                         : polyline::ChevronDirection::Right,
                2.f, chevron_color);
          }));
  div(ctx, mk(header.ent(), 1),
      ComponentConfig::inherit_from(config, "disclosure_label")
          .with_size(ComponentSize{expand(), percent(1.f)})
          .with_label(label)
          .with_alignment(TextAlignment::Left)
          .with_skip_tabbing(true));

  if (state.expanded) {
    auto body_container =
        div(ctx, mk(entity, 1),
            ComponentConfig::inherit_from(config, "disclosure_body")
                .with_size(ComponentSize{percent(1.f), children()})
                .with_flex_direction(FlexDirection::Column));
    body(body_container.ent());
  }

  return ElementResult{changed, entity};
}

} // namespace imm

} // namespace ui

} // namespace afterhours
