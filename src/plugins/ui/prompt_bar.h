#pragma once

#include <functional>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "component_config.h"
#include "element_result.h"
#include "entity_management.h"
#include "imm_components.h"

namespace afterhours {
namespace ui {
namespace imm {

struct PromptItem {
  std::string key;
  std::string label;
  std::function<void()> on_press = {};
};

inline ElementResult keycap(HasUIContext auto &ctx, EntityParent ep_pair,
                            std::string_view key,
                            ComponentConfig config = ComponentConfig()) {
  static constexpr float KEYCAP_HEIGHT = 28.f;
  static constexpr float KEYCAP_PAD_X = 8.f;

  config.with_label(std::string(key));
  if (config.label_alignment == TextAlignment::None)
    config.with_alignment(TextAlignment::Center);
  config.with_no_wrap().with_skip_tabbing(true);
  config.with_align_items(AlignItems::Center)
      .with_justify_content(JustifyContent::Center);

  if (config.size.is_default) {
    config.with_size(ComponentSize{children(), pixels(KEYCAP_HEIGHT)});
    if (config.min_width.dim == Dim::None)
      config.with_min_width(pixels(KEYCAP_HEIGHT));
  }
  if (!config.has_padding() && !config.padding_explicitly_set)
    config.with_padding(Padding::horizontal(pixels(KEYCAP_PAD_X)));
  if (config.color_usage == Theme::Usage::Default &&
      !config.custom_color.has_value())
    config.with_background(Theme::Usage::Surface);
  if (!config.has_border())
    config.with_border(ctx.theme.control_border(), pixels(1.f));
  if (!config.has_any_rounded_corners())
    config.with_rounded_corners(std::bitset<4>(0b1111));

  return div(ctx, ep_pair, config);
}

namespace detail {
inline ElementResult prompt_bar_impl(HasUIContext auto &ctx,
                                     EntityParent ep_pair,
                                     std::span<const PromptItem> items,
                                     ComponentConfig config,
                                     ComponentConfig keycap_config,
                                     ComponentConfig label_config) {
  static constexpr float BAR_GAP = 16.f;
  static constexpr float ITEM_GAP = 6.f;

  auto [entity, parent] = deref(ep_pair);

  ComponentConfig bar_config =
      ComponentConfig{}
          .with_size(ComponentSize{children(), children()})
          .with_flex_direction(FlexDirection::Row)
          .with_align_items(AlignItems::Center)
          .with_justify_content(JustifyContent::FlexStart)
          .with_gap(pixels(BAR_GAP))
          .with_padding(Padding::all(pixels(8.f)))
          .with_background(Theme::Usage::None)
          .with_debug_name("prompt_bar");
  bar_config.apply_overrides_from(config);
  bar_config.with_flex_direction(FlexDirection::Row);
  if (bar_config.align_items == AlignItems::FlexStart)
    bar_config.with_align_items(AlignItems::Center);
  if (bar_config.flex_gap.value == 0.f)
    bar_config.with_gap(pixels(BAR_GAP));
  if (bar_config.debug_name.empty())
    bar_config.with_debug_name("prompt_bar");

  auto bar = div(ctx, ep_pair, bar_config);
  Entity &bar_entity = bar.ent();

  bool clicked = false;
  int clicked_index = -1;

  for (size_t i = 0; i < items.size(); ++i) {
    const PromptItem &item = items[i];
    const bool interactive = static_cast<bool>(item.on_press);

    ComponentConfig item_config =
        ComponentConfig{}
            .with_size(ComponentSize{children(), children()})
            .with_flex_direction(FlexDirection::Row)
            .with_align_items(AlignItems::Center)
            .with_gap(pixels(ITEM_GAP))
            .with_padding(Padding{})
            .with_debug_name(fmt::format("prompt_item_{}", i));
    Entity *item_entity_ptr = nullptr;
    bool item_clicked = false;
    if (interactive) {
      item_config.with_custom_background(colors::transparent());
      auto item_result = button(ctx, mk(bar_entity, static_cast<int>(i)),
                                item_config);
      item_entity_ptr = &item_result.ent();
      item_clicked = static_cast<bool>(item_result);
    } else {
      item_config.with_background(Theme::Usage::None)
          .with_skip_tabbing(true);
      auto item_result = div(ctx, mk(bar_entity, static_cast<int>(i)),
                             item_config);
      item_entity_ptr = &item_result.ent();
    }
    Entity &item_entity = *item_entity_ptr;

    if (interactive && item_clicked) {
      clicked = true;
      clicked_index = static_cast<int>(i);
      item.on_press();
    }

    ComponentConfig resolved_keycap =
        ComponentConfig{}.with_debug_name(
            fmt::format("prompt_keycap_{}", i));
    if (config.has_font_override() || !config.font_size_is_default) {
      resolved_keycap.font_name = config.font_name;
      resolved_keycap.font_size = config.font_size;
      resolved_keycap.font_size_explicitly_set =
          config.font_size_explicitly_set;
      resolved_keycap.font_size_is_default = config.font_size_is_default;
    }
    resolved_keycap.apply_overrides_from(keycap_config);
    if (resolved_keycap.debug_name.empty() ||
        resolved_keycap.debug_name == "prompt_keycap_" ||
        keycap_config.debug_name.empty())
      resolved_keycap.with_debug_name(fmt::format("prompt_keycap_{}", i));
    else
      resolved_keycap.with_debug_name(
          fmt::format("{}_{}", keycap_config.debug_name, i));
    keycap(ctx, mk(item_entity, 0), item.key, resolved_keycap);

    ComponentConfig resolved_label =
        ComponentConfig{}
            .with_label(item.label)
            .with_size(ComponentSize{children(), children()})
            .with_alignment(TextAlignment::Left)
            .with_background(Theme::Usage::None)
            .with_skip_tabbing(true)
            .with_debug_name(fmt::format("prompt_label_{}", i));
    if (config.has_font_override() || !config.font_size_is_default) {
      resolved_label.font_name = config.font_name;
      resolved_label.font_size = config.font_size;
      resolved_label.font_size_explicitly_set =
          config.font_size_explicitly_set;
      resolved_label.font_size_is_default = config.font_size_is_default;
    }
    if (config.has_text_color_override()) {
      if (config.custom_text_color.has_value())
        resolved_label.with_custom_text_color(
            config.custom_text_color.value());
      else if (config.text_color_usage != Theme::Usage::Default)
        resolved_label.with_text_color(config.text_color_usage);
    }
    resolved_label.apply_overrides_from(label_config);
    resolved_label.with_label(item.label);
    if (label_config.debug_name.empty())
      resolved_label.with_debug_name(fmt::format("prompt_label_{}", i));
    else
      resolved_label.with_debug_name(
          fmt::format("{}_{}", label_config.debug_name, i));
    if (!resolved_label.has_label_alignment_override())
      resolved_label.with_alignment(TextAlignment::Left);

    label(ctx, mk(item_entity, 1), item.label, resolved_label);
  }

  return {clicked, bar_entity, clicked_index};
}
} // namespace detail

inline ElementResult prompt_bar(HasUIContext auto &ctx, EntityParent ep_pair,
                                std::span<const PromptItem> items,
                                ComponentConfig config = ComponentConfig(),
                                ComponentConfig keycap_config = ComponentConfig(),
                                ComponentConfig label_config = ComponentConfig()) {
  return detail::prompt_bar_impl(ctx, ep_pair, items, config, keycap_config,
                                 label_config);
}

inline ElementResult prompt_bar(HasUIContext auto &ctx, EntityParent ep_pair,
                                std::initializer_list<PromptItem> items,
                                ComponentConfig config = ComponentConfig(),
                                ComponentConfig keycap_config = ComponentConfig(),
                                ComponentConfig label_config = ComponentConfig()) {
  return detail::prompt_bar_impl(
      ctx, ep_pair, std::span<const PromptItem>(items.begin(), items.size()),
      config, keycap_config, label_config);
}

} // namespace imm
} // namespace ui
} // namespace afterhours
