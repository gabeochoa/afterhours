#pragma once

#include "console.h"
#include "../ui/text_input/component.h"

namespace afterhours::terminal {

namespace detail {

struct PanelState : BaseComponent {
  bool focus_requested = true;
  EntityID input_id = -1;
  EntityID field_id = -1;
  int follow_frames = 2;
  size_t output_revision = 0;
};

}

inline auto panel(ui::imm::HasUIContext auto &ctx, ui::imm::EntityParent parent,
                  Console &console,
                  ui::imm::ComponentConfig config = {}) {
  using namespace ui;
  using namespace ui::imm;
  using Action = typename std::remove_reference_t<decltype(ctx)>::value_type;

  auto &state = deref(parent).first.template addComponentIfMissing<detail::PanelState>();

  const bool focused = state.input_id >= 0 &&
      (ctx.has_focus(state.input_id) || ctx.has_focus(state.field_id));
  bool submit = false;
  if (focused) {
    if (ctx.pressed(Action::WidgetUp)) console.previous();
    if (ctx.pressed(Action::WidgetDown)) console.next();
    if (!ctx.is_held_down(Action::WidgetMod) && ctx.pressed(Action::WidgetNext)) {
      console.complete_input();
    }
    submit = ctx.pressed(Action::WidgetPress);
  }
  if (submit) console.submit();

  if (config.size.is_default) config.with_size({percent(1.f), h720(420.f)});
  if (config.font_size_is_default) config.with_font_size(h720(20.f));
  if (config.color_usage == Theme::Usage::Default) config.with_background(Theme::Usage::Surface);
  if (config.padding.top.dim == Dim::None && config.padding.left.dim == Dim::None &&
      config.padding.bottom.dim == Dim::None && config.padding.right.dim == Dim::None)
    config.with_padding(Padding::all(h720(16.f)));
  config.with_flex_direction(FlexDirection::Column).with_gap(h720(12.f));
  auto root = div(ctx, parent, config);
  auto child = ComponentConfig{}.with_font(config.font_name, config.font_size)
      .with_render_layer(config.render_layer).with_transparent_bg();
  auto output = div(ctx, mk(root.ent(), 0), child
      .with_size({percent(1.f), expand()})
      .with_flex_direction(FlexDirection::Column)
      .with_overflow(Overflow::Scroll, Axis::Y)
      .with_debug_name("terminal_output"));
  int index = 0;
  for (const auto &line : console.output()) {
    div(ctx, mk(output.ent(), index++), ComponentConfig{}
        .with_font(config.font_name, config.font_size)
        .with_render_layer(config.render_layer)
        .with_size({percent(1.f), h720(28.f)})
        .with_label(line.text).with_alignment(TextAlignment::Left)
        .with_custom_text_color(line.success ? ctx.theme.font : ctx.theme.error)
        .with_transparent_bg().with_ignore_pointer_events()
        .with_debug_name("terminal_line"));
  }
  if (state.follow_frames > 0 && output.ent().template has<HasScrollView>()) {
    auto &scroll = output.ent().template get<HasScrollView>();
    scroll.scroll_offset.y = std::max(0.f, scroll.content_size.y - scroll.viewport_or_zero().y);
    scroll.scroll_target.y = scroll.scroll_offset.y;
    --state.follow_frames;
  }

  auto row = div(ctx, mk(root.ent(), 1), ComponentConfig{}
      .with_size({percent(1.f), h720(44.f)})
      .with_flex_direction(FlexDirection::Row).with_gap(h720(12.f))
      .with_transparent_bg());
  auto input_parent = mk(row.ent(), 0);
  auto &input_entity = deref(input_parent).first;
  if (input_entity.template has<afterhours::text_input::HasTextInputState>()) {
    auto &input_state = input_entity.template get<afterhours::text_input::HasTextInputState>();
    if (input_state.text() != console.input) input_state.clear_selection();
  }
  auto field = afterhours::text_input::text_input(ctx, input_parent, console.input, ComponentConfig{}
      .with_font(config.font_name, config.font_size)
      .with_render_layer(config.render_layer)
      .with_size({expand(), percent(1.f)})
      .with_placeholder("Type help to see commands")
      .with_debug_name("terminal_input"));
  state.input_id = field.ent().id;
  for (auto id : field.cmp().children) {
    auto found = UICollectionHolder::getEntityForID(id);
    if (!found || !found.asE().template has<InFocusCluster>()) continue;
    state.field_id = id;
    break;
  }
  if (state.focus_requested) {
    ctx.set_focus(state.field_id);
    state.focus_requested = false;
  }
  if (button(ctx, mk(row.ent(), 1), ComponentConfig{}
      .with_font(config.font_name, config.font_size)
      .with_render_layer(config.render_layer)
      .with_size({h720(88.f), percent(1.f)})
      .with_label("Run").with_debug_name("terminal_run"))) {
    console.submit();
    state.focus_requested = true;
  }
  if (state.output_revision != console.output_revision()) {
    state.output_revision = console.output_revision();
    state.follow_frames = 2;
  }
  return root;
}

}
