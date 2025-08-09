#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <format>

#include "../../../entity.h"
#include "../../../entity_helper.h"
#include "../../input_system.h"
#include "../components.h"
#include "component_config.h"
#include "element_result.h"
#include "entity_management.h"
#include "rounded_corners.h"

namespace afterhours {

namespace ui {

namespace imm {

inline size_t prev_index(size_t current, size_t total) {
  return (current == 0) ? total - 1 : current - 1;
}

inline size_t next_index(size_t current, size_t total) {
  return (current + 1) % total;
}

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (config.size.is_default && config.label.empty())
    config.with_size(ComponentSize{children(), children()});
  if (config.size.is_default && !config.label.empty())
    config.with_size(ComponentSize{children(default_component_size.x),
                                   children(default_component_size.y)});

  _init_component(ctx, ep_pair, config, ComponentType::Div);

  return {true, entity};
}

ElementResult image(HasUIContext auto &ctx, EntityParent ep_pair,
                    ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image);

  return {false, entity};
}

ElementResult
sprite(HasUIContext auto &ctx, EntityParent ep_pair,
       afterhours::texture_manager::Texture texture,
       afterhours::texture_manager::Rectangle source_rect,
       afterhours::texture_manager::HasTexture::Alignment alignment =
           afterhours::texture_manager::HasTexture::Alignment::Center,
       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image);

  auto &img = entity.addComponentIfMissing<ui::HasImage>(texture, source_rect,
                                                         alignment);
  img.texture = texture;
  img.source_rect = source_rect;
  img.alignment = alignment;

  return {false, entity};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Button, true, "button");

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

template <typename Container>
ElementResult button_group(HasUIContext auto &ctx, EntityParent ep_pair,
                           const Container &labels,
                           ComponentConfig config = ComponentConfig()) {

  auto [entity, parent] = deref(ep_pair);

  auto max_height = config.size.y_axis;
  config.size.y_axis = children(max_height.value);
  auto max_width = config.size.x_axis;
  config.size.x_axis = children(max_width.value);

  _init_component(ctx, ep_pair, config, ComponentType::ButtonGroup, false,
                  "button_group");

  config.size.x_axis = config.flex_direction == FlexDirection::Row
                           ? pixels(max_width.value / labels.size())
                           : max_width;
  config.size.y_axis = config.flex_direction == FlexDirection::Row
                           ? max_height
                           : children(max_height.value);

  entity.get<UIComponent>().flex_direction = config.flex_direction;

  bool clicked = false;
  int value = -1;
  for (size_t i = 0; i < labels.size(); ++i) {
    if (button(
            ctx, mk(entity, i),
            ComponentConfig::inherit_from(config,
                                          std::format("button group {}", i))
                .with_size(config.size)
                .with_label(i < labels.size() ? std::string(labels[i]) : ""))) {
      clicked = true;
      value = static_cast<int>(i);
    }
  }

  return {clicked, entity, value};
}

ElementResult checkbox_no_label(HasUIContext auto &ctx, EntityParent ep_pair,
                                bool &value,
                                ComponentConfig config = ComponentConfig()) {

  auto [entity, parent] = deref(ep_pair);

  HasCheckboxState &checkboxState = _init_state<HasCheckboxState>(
      entity, [&](auto &) {}, value);

  config.label = value ? "X" : " ";

  _init_component(ctx, ep_pair, config, ComponentType::CheckboxNoLabel, true,
                  "checkbox");

  if (config.disabled) {
    entity.removeComponentIfExists<HasClickListener>();
  } else {
    entity.addComponentIfMissing<HasClickListener>([](Entity &ent) {
      auto &cbs = ent.get<HasCheckboxState>();
      cbs.on = !cbs.on;
      cbs.changed_since = true;
    });
  }

  value = checkboxState.on;
  ElementResult result{checkboxState.changed_since, entity, value};
  checkboxState.changed_since = false;
  return result;
}

// TODO the focus ring is not correct because the actual clickable area is the
// checkbox_no_label element, not the checkbox element.
ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                       bool &value,
                       ComponentConfig config = ComponentConfig()) {

  auto [entity, parent] = deref(ep_pair);

  auto label = config.label;
  config.label = "";

  _init_component(ctx, ep_pair, config, ComponentType::Div, false,
                  "checkbox_row");

  config.size = ComponentSize(pixels(default_component_size.x),
                              children(default_component_size.y));
  if (!label.empty()) {
    config.size = config.size._scale_x(0.5f);

    auto label_config =
        ComponentConfig::inherit_from(
            config, std::format("checkbox label {}", config.debug_name))
            .with_size(config.size)
            .with_label(label);

    // TODO - if the user wants to mess with the corners, how can we merge these
    if (config.color_usage == Theme::Usage::Default)
      label_config.with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(RoundedCorners().right_sharp());

    div(ctx, mk(entity), label_config);
  }

  auto checkbox_config =
      ComponentConfig::inherit_from(
          config, std::format("checkbox indiv from {}", config.debug_name))
          .with_size(config.size);

  if (config.color_usage == Theme::Usage::Default)
    checkbox_config.with_color_usage(Theme::Usage::Primary)
        .with_rounded_corners(RoundedCorners().left_sharp());

  bool changed = false;
  if (checkbox_no_label(ctx, mk(entity), value, checkbox_config)) {
    changed = true;
  }

  return {changed, entity, value};
}

template <size_t Size>
ElementResult
checkbox_group(HasUIContext auto &ctx, EntityParent ep_pair,
               std::bitset<Size> &values,
               const std::array<std::string_view, Size> &labels = {{}},
               std::pair<int, int> min_max = {-1, -1},
               ComponentConfig config = ComponentConfig()) {

  auto [entity, parent] = deref(ep_pair);

  auto max_height = config.size.y_axis;
  config.size.y_axis = children();
  _init_component(ctx, ep_pair, config, ComponentType::CheckboxGroup, false,
                  "checkbox_group");
  config.size.y_axis = max_height;

  int count = (int)values.count();

  const auto should_disable = [min_max, count](bool value) -> bool {
    // we should disable, if not checked and we are at the cap
    bool at_cap = !value && min_max.second != -1 && count >= min_max.second;
    // we should disable, if checked and we are at the min
    bool at_min = value && min_max.first != -1 && count <= min_max.first;
    return at_cap || at_min;
  };

  bool changed = false;
  for (size_t i = 0; i < values.size(); ++i) {
    bool value = values.test(i);

    if (checkbox(
            ctx, mk(entity, i), value,
            ComponentConfig::inherit_from(config,
                                          std::format("checkbox row {}", i))
                .with_size(config.size)
                .with_label(i < labels.size() ? std::string(labels[i]) : "")
                .with_color_usage(Theme::Usage::None)
                .with_flex_direction(FlexDirection::Row)
                .with_disabled(should_disable(value))
                .with_render_layer(config.render_layer))) {
      changed = true;
      if (value)
        values.set(i);
      else
        values.reset(i);
    }
  }

  return {changed, entity, values};
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  ElementResult result = {false, entity};

  std::string original_label = config.label;
  config.label = "";

  _init_component(ctx, ep_pair, config, ComponentType::Slider, true, "slider");

  auto label_corners = RoundedCorners(config.rounded_corners.value())
                           .sharp(TOP_RIGHT)
                           .sharp(BOTTOM_RIGHT);

  auto label = div(ctx, mk(entity, entity.id + 0),
                   ComponentConfig::inherit_from(config, "slider_text")
                       .with_size(config.size)
                       .with_label(original_label)
                       .with_color_usage(Theme::Usage::Primary)
                       .with_rounded_corners(label_corners)
                       .with_render_layer(config.render_layer + 0));
  label.ent()
      .template get<UIComponent>()
      .set_desired_width(config.size._scale_x(0.5f).x_axis)
      .set_desired_height(config.size.y_axis);

  auto elem_corners =
      RoundedCorners(config.rounded_corners.value()).left_sharp();

  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0),
                  ComponentConfig::inherit_from(config, "slider_background")
                      .with_size(config.size)
                      .with_color_usage(Theme::Usage::Secondary)
                      .with_rounded_corners(elem_corners)
                      .with_render_layer(config.render_layer + 1));

  elem.ent().template get<UIComponent>().set_desired_width(config.size.x_axis);

  Entity &slider_bg = elem.ent();
  if (slider_bg.is_missing<ui::HasSliderState>())
    slider_bg.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = slider_bg.get<ui::HasSliderState>();
  sliderState.changed_since = true;

  {
    slider_bg.addComponentIfMissing<ui::HasDragListener>(
        [&sliderState](Entity &draggable) {
          float mnf = 0.f;
          float mxf = 1.f;

          UIComponent &cmp = draggable.get<UIComponent>();
          Rectangle rect = cmp.rect();

          auto mouse_position = input::get_mouse_position();
          float v =
              std::clamp((mouse_position.x - rect.x) / rect.width, mnf, mxf);

          if (v != sliderState.value) {
            sliderState.value = v;
            sliderState.changed_since = true;
          }

          auto opt_child =
              EntityQuery()
                  .whereID(draggable.get<UIComponent>().children[0])
                  .gen_first();

          UIComponent &child_cmp = opt_child->get<UIComponent>();
          child_cmp.set_desired_padding(
              pixels(sliderState.value * 0.75f * rect.width), Axis::left);
        });
  }

  auto handle_corners = config.rounded_corners.value();

  auto handle_config =
      ComponentConfig::inherit_from(config, "slider_handle")
          .with_size(ComponentSize{pixels(0.25f * config.size.x_axis.value),
                                   config.size.y_axis})
          .with_padding(Padding{
              .left = pixels(owned_value * 0.75f * config.size.x_axis.value)})
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(handle_corners)
          .with_debug_name("slider_handle")
          .with_render_layer(config.render_layer + 2);

  auto handle = div(ctx, mk(slider_bg), handle_config);

  handle.cmp()
      .set_desired_width(pixels(0.25f * config.size.x_axis.value))
      .set_desired_height(config.size.y_axis);

  owned_value = sliderState.value;
  return ElementResult{sliderState.changed_since, entity, sliderState.value};
}

template <typename Container>
ElementResult pagination(HasUIContext auto &ctx, EntityParent ep_pair,
                         const Container &options, size_t &option_index,
                         ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  if (entity.is_missing<ui::HasDropdownState>())
    entity.addComponent<ui::HasDropdownState>(
        options, nullptr, [&](size_t opt) {
          HasDropdownState &ds = entity.get<ui::HasDropdownState>();
          if (!ds.on) {
            ds.last_option_clicked = opt;
          }
        });
  HasDropdownState &dropdownState = entity.get<ui::HasDropdownState>();
  dropdownState.last_option_clicked = (size_t)option_index;
  dropdownState.changed_since = false;

  const auto on_option_click = [options, &ctx](Entity &dd, size_t i) {
    size_t index = i % options.size();
    HasDropdownState &ds = dd.get<ui::HasDropdownState>();
    ds.last_option_clicked = index;
    ds.on = !ds.on;
    ds.changed_since = true;

    EntityID id = dd.get<UIComponent>().children[i];
    ctx.set_focus(id);
  };

  if (config.size.is_default) {
    config.size = ComponentSize(children(default_component_size.x),
                                pixels(default_component_size.y));
  }
  config.flex_direction = FlexDirection::Row;

  std::string label_str = config.label;
  config.label = "";

  bool first_time = _init_component(
      ctx, ep_pair, config, ComponentType::Pagination, false, "pagination");

  int child_index = 0;

  if (button(
          ctx, mk(entity),
          ComponentConfig::inherit_from(config, "left")
              .with_size(ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis})
              .with_label("<")
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, prev_index(option_index - 1, options.size()));
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(
            ctx, mk(entity, child_index + i),
            ComponentConfig::inherit_from(config,
                                          std::format("option {}", i + 1))
                .with_size(ComponentSize{pixels(default_component_size.x / 2.f),
                                         config.size.y_axis})
                .with_label(std::string(options[i]))
                .with_render_layer(config.render_layer + 1))) {
      on_option_click(entity, i + 1);
    }
  }

  if (button(
          ctx, mk(entity),
          ComponentConfig::inherit_from(config, "right")
              .with_size(ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis})
              .with_label(">")
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, next_index(option_index, options.size()));
  }

  if (first_time) {
    EntityID id = entity.get<UIComponent>()
                      .children[dropdownState.last_option_clicked + 1];
    ctx.set_focus(id);
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

template <typename Container>
ElementResult dropdown(HasUIContext auto &ctx, EntityParent ep_pair,
                       const Container &options, size_t &option_index,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasDropdownState &dropdownState = _init_state<HasDropdownState>(
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
    config.size = ComponentSize(children(default_component_size.x),
                                pixels(default_component_size.y));
  }

  std::string label_str = config.label;
  config.label = "";
  config.flex_direction = FlexDirection::Row;

  _init_component(ctx, ep_pair, config, ComponentType::Dropdown);

  auto button_corners = std::bitset<4>(config.rounded_corners.value());

  auto config_size = config.size;

  if (!label_str.empty()) {
    config_size = config.size._scale_x(0.5f);
    button_corners = RoundedCorners(button_corners).left_sharp();

    auto label = div(
        ctx, mk(entity),
        ComponentConfig::inherit_from(config, "dropdown_label")
            .with_size(config_size)
            .with_label(std::string(label_str))
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(
                RoundedCorners(config.rounded_corners.value()).right_sharp())
            .with_render_layer(config.render_layer + 0));
  }

  const auto toggle_visibility = [&](Entity &) {
    dropdownState.on = !dropdownState.on;
  };

  const auto on_option_click = [&](Entity &dd, size_t option_index) {
    toggle_visibility(dd);
    dropdownState.last_option_clicked = option_index;
    dropdownState.changed_since = true;

    EntityID id = entity.get<UIComponent>().children[label_str.empty() ? 0 : 1];
    Entity &first_child = EntityHelper::getEntityForIDEnforce(id);

    first_child.get<ui::HasLabel>().label = (options[option_index]);
    ctx.set_focus(first_child.id);
  };

  auto current_option = std::string(
      options[dropdownState.on ? 0 : dropdownState.last_option_clicked]);
  auto drop_arrow_icon = dropdownState.on ? " ^" : " V";
  auto main_button_label = std::format("{}{}", current_option, drop_arrow_icon);
  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "option 1")
                 .with_size(config_size)
                 .with_label(main_button_label)
                 .with_rounded_corners(button_corners)
                 // TODO This works great but we need a way to
                 // close the dropdown when you leave without selecting anything
                 //  .with_select_on_focus(true)
                 .with_render_layer(config.render_layer +
                                    (dropdownState.on ? 0 : 0)))) {

    dropdownState.on ? on_option_click(entity, 0) : toggle_visibility(entity);
  }

  if (auto result = button_group(
          ctx, mk(entity), options,
          ComponentConfig::inherit_from(config, "dropdown button group")
              .with_hidden(config.hidden || !dropdownState.on)
              .with_render_layer(config.render_layer + 1));
      result) {
    on_option_click(entity, result.template as<int>());
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

template <typename Container>
ElementResult navigation_bar(HasUIContext auto &ctx, EntityParent ep_pair,
                             const Container &options, size_t &option_index,
                             ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (options.empty())
    return {false, entity};

  HasNavigationBarState &navState = _init_state<HasNavigationBarState>(
      entity,
      [&](auto &hnbs) {
        hnbs.set_current_index(option_index);
        hnbs.changed_since = false;
      },
      options, nullptr);

  if (config.size.is_default) {
    config.size = ComponentSize(pixels(default_component_size.x),
                                pixels(default_component_size.y));
  }
  // TODO - add default
  config.flex_direction = FlexDirection::Row;

  _init_component(ctx, ep_pair, config, ComponentType::NavigationBar, false,
                  "navigation_bar");

  bool clicked = false;
  size_t new_index = navState.current_index();

  constexpr float arrow_ratio = 0.20f;
  constexpr float label_ratio = 1.f - (arrow_ratio * 2.f); // 60 % for label

  auto arrow_size = ComponentSize{percent(arrow_ratio), config.size.y_axis};

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "left_arrow")
                 .with_size(arrow_size)
                 .with_label("<")
                 .with_rounded_corners(RoundedCorners().left_round()))) {
    clicked = true;
    new_index = prev_index(navState.current_index(), options.size());
  }

  div(ctx, mk(entity),
      ComponentConfig::inherit_from(config, "center_label")
          .with_size(ComponentSize{percent(label_ratio), config.size.y_axis})
          .with_label(std::string(options[navState.current_index()]))
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(RoundedCorners().all_sharp())
          .with_skip_tabbing(true));

  if (button(ctx, mk(entity),
             ComponentConfig::inherit_from(config, "right_arrow")
                 .with_size(arrow_size)
                 .with_label(">")
                 .with_rounded_corners(RoundedCorners().right_round()))) {
    clicked = true;
    new_index = next_index(navState.current_index(), options.size());
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

} // namespace imm

} // namespace ui

} // namespace afterhours
