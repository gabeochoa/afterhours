#pragma once

#include "picker.h"
#include "../ui/text_input/component.h"
#include "../ui/imm_components.h"

namespace afterhours::command_picker {

struct Style {
  ui::imm::ComponentConfig search;
  ui::imm::ComponentConfig row;
  ui::imm::ComponentConfig selected_row;
  ui::imm::ComponentConfig detail;
};

namespace detail {

struct PanelState : BaseComponent {
  EntityID input_id = -1;
  EntityID field_id = -1;
  bool focus_requested = true;
};

}

inline bool panel(ui::imm::HasUIContext auto &ctx, ui::imm::EntityParent parent,
                  terminal::Console &console, Picker &picker,
                  ui::imm::ComponentConfig config = {}, const Style &style = {}) {
  using namespace ui;
  using namespace ui::imm;
  using Action = typename std::remove_reference_t<decltype(ctx)>::value_type;
  auto &state = deref(parent).first.template addComponentIfMissing<detail::PanelState>();
  bool reveal = picker.refresh();
  bool activate = false;
  const bool focused = state.input_id >= 0 &&
      (ctx.has_focus(state.input_id) || ctx.has_focus(state.field_id));
  if (focused) {
    if (ctx.pressed(Action::WidgetUp)) { picker.move(false); reveal = true; }
    if (ctx.pressed(Action::WidgetDown)) { picker.move(true); reveal = true; }
    activate = ctx.pressed(Action::WidgetPress);
    if (ctx.pressed(Action::MenuBack)) {
      picker.query.clear();
      reveal = picker.refresh();
    }
  }

  const auto name = config.debug_name.empty() ? std::string("command_picker") : config.debug_name;
  if (config.size.is_default) config.with_size({percent(1.f), h720(480.f)});
  if (config.font_size_is_default) config.with_font_size(h720(20.f));
  if (config.color_usage == Theme::Usage::Default) config.with_background(Theme::Usage::Surface);
  config.with_flex_direction(FlexDirection::Column).with_gap(h720(8.f));
  auto root = div(ctx, parent, config);
  const auto base = ComponentConfig{}.with_font(config.font_name, config.font_size)
      .with_render_layer(config.render_layer).with_transparent_bg();
  auto search_config = base;
  auto field = afterhours::text_input::text_input(ctx, mk(root.ent(), 0), picker.query,
      search_config.with_size({percent(1.f), h720(44.f)})
          .with_placeholder("Search commands...").apply_overrides(style.search)
          .with_debug_name(name + "_search"));
  state.input_id = field.ent().id;
  for (auto id : field.cmp().children) {
    auto found = UICollectionHolder::getEntityForID(id);
    if (!found || !found.asE().template has<InFocusCluster>()) continue;
    state.field_id = id;
    break;
  }
  if (state.focus_requested) {
    if (ctx.is_input_allowed(state.field_id)) ctx.set_focus(state.field_id);
    state.focus_requested = false;
  }
  reveal = picker.refresh() || reveal;

  auto list_parent = mk(root.ent(), 1);
  auto &list_entity = deref(list_parent).first;
  const float row_height = 64.f * ctx.screen_height / 720.f;
  if (reveal && list_entity.template has<HasScrollView>()) {
    auto &scroll = list_entity.template get<HasScrollView>();
    const float row_scale = list_entity.template get<UIComponent>().resolved_scaling_mode == ScalingMode::Adaptive
        ? ctx.theme.ui_scale : 1.f;
    const float top = static_cast<float>(picker.selected()) * row_height * row_scale;
    const float bottom = top + row_height * row_scale;
    const float height = scroll.viewport_or_zero().y;
    if (top < scroll.scroll_offset.y) scroll.scroll_offset.y = top;
    if (bottom > scroll.scroll_offset.y + height) scroll.scroll_offset.y = std::max(0.f, bottom - height);
    scroll.scroll_target.y = scroll.scroll_offset.y;
  }
  auto list_config = base;
  virtual_list(ctx, list_parent, picker.count(), row_height, [&](size_t i, Entity &row) {
    const auto &entry = picker.entry(i);
    const auto parsed = terminal::detail::parse(entry.command);
    const std::string command_name = parsed.words.empty() ? std::string{} : parsed.words.front();
    std::string description(console.command_help(command_name));
    if (const auto reason = console.command_unavailable_reason(command_name))
      description = "Unavailable: " + *reason;
    auto row_config = base;
    row_config.with_size({percent(1.f), percent(1.f)}).with_corner_radius(0.f)
        .with_padding(Padding::all(pixels(0.f)))
        .with_custom_hover_bg(ctx.theme.secondary);
    row_config = row_config.apply_overrides(style.row);
    if (i == picker.selected()) {
      row_config.with_custom_background(ctx.theme.secondary)
          .with_border_left(ctx.theme.accent, h720(2.f));
      row_config = row_config.apply_overrides(style.selected_row);
    }
    auto option = button(ctx, mk(row, 0), row_config
        .with_flex_direction(FlexDirection::Row).with_skip_tabbing(true)
        .with_debug_name(name + "_row_" + std::to_string(i)));
    auto column_config = base;
    auto column = div(ctx, mk(option.ent(), 0), column_config
        .with_size({percent(0.65f), percent(1.f)})
        .with_flex_direction(FlexDirection::Column));
    auto text = ComponentConfig{}.with_font(row_config.font_name, row_config.font_size)
        .with_render_layer(config.render_layer).with_transparent_bg();
    text.with_size({percent(1.f), percent(0.5f)}).with_alignment(TextAlignment::Left)
        .with_text_overflow(TextOverflow::Ellipsis).with_ignore_pointer_events();
    auto title = text;
    div(ctx, mk(column.ent(), 0), title.with_label(entry.label)
        .with_custom_text_color(row_config.custom_text_color.value_or(
            i == picker.selected() ? ctx.theme.accent : ctx.theme.font)));
    auto description_config = text;
    div(ctx, mk(column.ent(), 1), description_config.with_label(description)
        .with_custom_text_color(ctx.theme.font_muted).apply_overrides(style.detail));
    auto category = text;
    div(ctx, mk(option.ent(), 1), category.with_size({percent(0.2f), percent(1.f)})
        .with_label(entry.category).with_custom_text_color(ctx.theme.font_muted));
    auto shortcut = text;
    div(ctx, mk(option.ent(), 2), shortcut.with_size({percent(0.15f), percent(1.f)})
        .with_label(entry.shortcut).with_custom_text_color(ctx.theme.font_muted));
    if (!option) return;
    picker.select(i);
    activate = true;
    state.focus_requested = true;
  }, list_config.with_size({percent(1.f), expand()}).with_debug_name(name + "_list"));

  const bool accepted = activate && picker.activate(console);
  picker.refresh();
  std::string hint = "No matching commands";
  if (picker.count()) {
    const auto parsed = terminal::detail::parse(picker.entry(picker.selected()).command);
    if (!parsed.words.empty()) {
      hint = "Usage: " + std::string(console.command_usage(parsed.words.front()));
      if (const auto reason = console.command_unavailable_reason(parsed.words.front()))
        hint = "Unavailable: " + *reason;
    }
  }
  auto hint_config = base;
  div(ctx, mk(root.ent(), 2), hint_config.with_size({percent(1.f), h720(28.f)})
      .with_label(hint).with_custom_text_color(ctx.theme.font_muted)
      .with_alignment(TextAlignment::Left).with_text_overflow(TextOverflow::Ellipsis)
      .with_ignore_pointer_events().apply_overrides(style.detail)
      .with_debug_name(name + "_hint"));
  auto result_config = base;
  div(ctx, mk(root.ent(), 3), result_config.with_size({percent(1.f), h720(28.f)})
      .with_label(picker.result.text).with_custom_text_color(picker.result.success ? ctx.theme.font : ctx.theme.error)
      .with_alignment(TextAlignment::Left).with_text_overflow(TextOverflow::Ellipsis)
      .with_ignore_pointer_events().with_debug_name(name + "_result"));
  return accepted;
}

}
