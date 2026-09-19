#pragma once

#include "autocomplete.h"
#include "../ui/text_input/component.h"

namespace afterhours::terminal {

struct AutocompleteStyle {
  ui::imm::ComponentConfig list;
  ui::imm::ComponentConfig row;
  ui::imm::ComponentConfig selected_row;
  ui::imm::ComponentConfig description;
};

namespace detail {

struct PanelState : BaseComponent {
  Autocomplete autocomplete;
  std::vector<EntityID> suggestion_ids;
  bool focus_requested = true;
  EntityID input_id = -1;
  EntityID field_id = -1;
  int follow_frames = 2;
  size_t output_revision = 0;
};

}

inline auto panel(ui::imm::HasUIContext auto &ctx, ui::imm::EntityParent parent,
                  Console &console,
                  ui::imm::ComponentConfig config = {},
                  const AutocompleteStyle &autocomplete_style = {}) {
  using namespace ui;
  using namespace ui::imm;
  using Action = typename std::remove_reference_t<decltype(ctx)>::value_type;

  const auto name = config.debug_name.empty() ? std::string("terminal") : config.debug_name;
  auto &state = deref(parent).first.template addComponentIfMissing<detail::PanelState>();

  const bool focused = state.input_id >= 0 &&
      (ctx.has_focus(state.input_id) || ctx.has_focus(state.field_id));
  auto &completion = state.autocomplete;
  completion.refresh(console);
  const bool suggestion_active = std::any_of(state.suggestion_ids.begin(),
      state.suggestion_ids.end(), [&](EntityID id) { return ctx.has_focus(id) || ctx.was_hot(id); });
  if (!focused && !suggestion_active && !state.focus_requested) completion.dismiss(console);
  if (focused) {
    if (!completion.matches.empty() && ctx.pressed(Action::MenuBack)) completion.dismiss(console);
    if (ctx.pressed(Action::WidgetUp)) {
      if (!completion.matches.empty()) completion.move(false);
      else { console.previous(); completion.dismiss(console); }
    }
    if (ctx.pressed(Action::WidgetDown)) {
      if (!completion.matches.empty()) completion.move(true);
      else { console.next(); completion.dismiss(console); }
    }
    if (!ctx.is_held_down(Action::WidgetMod) && ctx.pressed(Action::WidgetNext)) {
      if (completion.matches.empty()) completion.refresh(console, true);
      completion.accept(console);
    }
    if (ctx.pressed(Action::WidgetPress)) {
      if (!completion.matches.empty() &&
          (console.enter_accepts_first_suggestion || completion.explicitly_selected))
        completion.accept(console);
      else console.submit();
    }
  }

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
      .with_debug_name(name + "_output"));
  int index = 0;
  for (const auto &line : console.output()) {
    div(ctx, mk(output.ent(), index++), ComponentConfig{}
        .with_font(config.font_name, config.font_size)
        .with_render_layer(config.render_layer)
        .with_size({percent(1.f), h720(28.f)})
        .with_label(line.text).with_alignment(TextAlignment::Left)
        .with_custom_text_color(line.success ? ctx.theme.font : ctx.theme.error)
        .with_transparent_bg().with_ignore_pointer_events()
        .with_debug_name(name + "_line"));
  }
  if (state.follow_frames > 0 && output.ent().template has<HasScrollView>()) {
    auto &scroll = output.ent().template get<HasScrollView>();
    scroll.scroll_offset.y = std::max(0.f, scroll.content_size.y - scroll.viewport_or_zero().y);
    scroll.scroll_target.y = scroll.scroll_offset.y;
    --state.follow_frames;
  }

  state.suggestion_ids.clear();
  if (!completion.matches.empty()) {
    const size_t visible = std::min(size_t{5}, completion.matches.size());
    const size_t first = completion.selected >= visible ? completion.selected - visible + 1 : 0;
    const float scale = ctx.screen_height / 720.f;
    auto list_config = child
        .with_size({percent(1.f), children()})
        .with_border_top(ctx.theme.control_border(ctx.theme.surface), pixels(1.f))
        .with_padding(Padding{.top = h720(8.f), .bottom = h720(4.f)})
        .with_corner_radius(0)
        .apply_overrides(autocomplete_style.list)
        .with_flex_direction(FlexDirection::Column)
        .with_debug_name(name + "_suggestions");
    auto suggestions = div(ctx, mk(root.ent(), 2), list_config);
    for (size_t i = first; i < first + visible; ++i) {
      const bool selected = i == completion.selected;
      auto row_config = ComponentConfig{}
          .with_font(config.font_name, config.font_size)
          .with_render_layer(config.render_layer)
          .with_size({percent(1.f), h720(32.f)})
          .with_padding(Padding::all(pixels(0.f)))
          .with_text_inset(10.f * scale, 0.f)
          .with_corner_radius(0)
          .with_transparent_bg().with_custom_hover_bg(ctx.theme.secondary)
          .with_custom_text_color(ctx.theme.font)
          .apply_overrides(autocomplete_style.row);
      if (selected) {
        row_config.with_custom_text_color(ctx.theme.accent)
            .with_border_left(ctx.theme.accent, h720(2.f));
        row_config = row_config.apply_overrides(autocomplete_style.selected_row);
      }
      auto option = button(ctx, mk(suggestions.ent(), static_cast<int>(i)), row_config
          .with_flex_direction(FlexDirection::Row).with_alignment(TextAlignment::Left)
          .with_skip_tabbing(true)
          .with_debug_name(name + "_suggestion_" + std::to_string(i)));
      auto text_config = ComponentConfig::inherit_from(row_config)
          .with_custom_text_color(row_config.custom_text_color.value_or(ctx.theme.font))
          .with_text_inset(row_config.text_inset->x, row_config.text_inset->y)
          .with_transparent_bg().with_ignore_pointer_events()
          .with_render_layer(config.render_layer)
          .with_alignment(TextAlignment::Left).with_text_overflow(TextOverflow::Ellipsis);
      text_config.border_config.reset();
      div(ctx, mk(option.ent(), 0), text_config
          .with_size({percent(0.22f), percent(1.f)})
          .with_label(completion.matches[i])
          .with_debug_name(name + "_suggestion_name_" + std::to_string(i)));
      const std::string_view match = completion.matches[i];
      const auto command_name = match.substr(0, match.find(' '));
      std::string description(console.command_help(command_name));
      if (const auto reason = console.command_unavailable_reason(command_name))
        description = "Unavailable: " + *reason;
      auto description_config = text_config;
      if (!selected) description_config.with_custom_text_color(ctx.theme.font_muted);
      div(ctx, mk(option.ent(), 1), description_config
          .apply_overrides(autocomplete_style.description)
          .with_size({expand(), percent(1.f)})
          .with_label(description)
          .with_debug_name(name + "_suggestion_description_" + std::to_string(i)));
      state.suggestion_ids.push_back(option.ent().id);
      if (!option) continue;
      completion.selected = i;
      completion.accept(console);
      state.focus_requested = true;
      break;
    }
  }

  const auto typed = detail::parse(console.input, true);
  std::string hint_command = typed.words.empty() ? std::string{} : typed.words.front();
  if ((completion.explicitly_selected || console.command_usage(hint_command).empty()) &&
      !completion.matches.empty()) {
    const auto selected = detail::parse(completion.matches[completion.selected], true);
    if (!selected.words.empty()) hint_command = selected.words.front();
  }
  const std::string usage(console.command_usage(hint_command));
  if (!usage.empty()) {
    auto hint_config = ComponentConfig{}
        .with_font(config.font_name, config.font_size)
        .with_render_layer(config.render_layer).with_transparent_bg()
        .with_size({percent(1.f), h720(28.f)})
        .with_alignment(TextAlignment::Left).with_text_overflow(TextOverflow::Ellipsis)
        .with_ignore_pointer_events();
    div(ctx, mk(root.ent(), 3), hint_config
        .with_label("Usage: " + usage).with_custom_text_color(ctx.theme.font_muted)
        .with_debug_name(name + "_usage"));
    if (const auto reason = console.command_unavailable_reason(hint_command)) {
      div(ctx, mk(root.ent(), 4), hint_config
          .with_label("Unavailable: " + *reason).with_custom_text_color(ctx.theme.error)
          .with_debug_name(name + "_unavailable"));
    }
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
      .with_debug_name(name + "_input"));
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
  if (button(ctx, mk(row.ent(), 1), ComponentConfig{}
      .with_font(config.font_name, config.font_size)
      .with_render_layer(config.render_layer)
      .with_size({h720(88.f), percent(1.f)})
      .with_label("Run").with_debug_name(name + "_run"))) {
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
