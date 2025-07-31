#pragma once

#include <array>
#include <bitset>
#include <format>

#include "../../../entity.h"
#include "../../../entity_helper.h"
#include "../../../logging.h"
#include "../../input_system.h"
#include "../components.h"
#include "../context.h"
#include "../theme.h"
#include "component_config.h"
#include "element_result.h"
#include "entity_management.h"
#include "rounded_corners.h"

namespace afterhours {

namespace ui {

namespace imm {

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  if (config.size.is_default && config.label.empty())
    config.with_size(ComponentSize{children(), children()});
  if (config.size.is_default && !config.label.empty())
    config.with_size(ComponentSize{children(default_component_size.x),
                                   children(default_component_size.y)});

  config = _overwrite_defaults(ctx, config);
  _init_component(ctx, entity, parent, config);

  return {true, entity};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  config = _overwrite_defaults(ctx, config, true);
  _init_component(ctx, entity, parent, config, "button");

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  return ElementResult{entity.get<HasClickListener>().down, entity};
}

template <typename Container>
ElementResult button_group(HasUIContext auto &ctx, EntityParent ep_pair,
                           const Container &labels,
                           ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto max_height = config.size.y_axis;
  config.size.y_axis = children(max_height.value);
  auto max_width = config.size.x_axis;
  config.size.x_axis = children(max_width.value);

  _init_component(ctx, entity, parent, config, "button_group");

  config.size.x_axis = config.flex_direction == FlexDirection::Row
                           ? pixels(max_width.value / labels.size())
                           : max_width;
  config.size.y_axis = config.flex_direction == FlexDirection::Row
                           ? max_height
                           : children(max_height.value);

  entity.get<UIComponent>().flex_direction = config.flex_direction;

  bool clicked = false;
  int value = -1;
  for (int i = 0; i < labels.size(); i++) {
    if (button(
            ctx, mk(entity, i),
            ComponentConfig::inherit_from(config,
                                          std::format("button group {}", i))
                .with_size(config.size)
                .with_label(i < labels.size() ? std::string(labels[i]) : ""))) {
      clicked = true;
      value = i;
    }
  }

  return {clicked, entity, value};
}

ElementResult checkbox(HasUIContext auto &ctx, EntityParent ep_pair,
                       bool &value,
                       ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  HasCheckboxState &checkboxState = _init_state<HasCheckboxState>(
      entity, [&](auto &) {}, value);

  config.label = value ? "X" : " ";

  config = _overwrite_defaults(ctx, config, true);
  _init_component(ctx, entity, parent, config, "checkbox");

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

ElementResult checkbox_group_row(HasUIContext auto &ctx, EntityParent ep_pair,
                                 int index, bool &value,
                                 ComponentConfig config = ComponentConfig()) {

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto label = config.label;
  config.label = "";

  _init_component(ctx, entity, parent, config, "checkbox_row");

  config.size = ComponentSize(pixels(default_component_size.x),
                              children(default_component_size.y));
  if (!label.empty()) {
    config.size = config.size._scale_x(0.5f);

    div(ctx, mk(entity),
        ComponentConfig::inherit_from(config,
                                      std::format("checkbox label {}", index))
            .with_size(config.size)
            .with_label(label));
  }

  bool changed = false;
  if (checkbox(ctx, mk(entity), value,
               ComponentConfig::inherit_from(config,
                                             std::format("checkbox {}", index))
                   .with_size(config.size))) {
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

  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  auto max_height = config.size.y_axis;
  config.size.y_axis = children();
  _init_component(ctx, entity, parent, config, "checkbox_group");
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
  for (int i = 0; i < values.size(); i++) {
    bool value = values.test(i);

    if (checkbox_group_row(
            ctx, mk(entity, i), i, value,
            ComponentConfig{
                .size = config.size,
                .label = i < labels.size() ? std::string(labels[i]) : "",
                .flex_direction = FlexDirection::Row,
                // inheritables
                .label_alignment = config.label_alignment,
                .skip_when_tabbing = config.skip_when_tabbing,
                .disabled = should_disable(value),
                .hidden = config.hidden,
                // debugs
                .debug_name = std::format("checkbox row {}", i),
                .render_layer = config.render_layer,

            })) {
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
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

  ElementResult result = {false, entity};

  std::string original_label = config.label;
  config.label = "";

  config = _overwrite_defaults(ctx, config, true);
  _init_component(ctx, entity, parent, config, "slider");

  auto label_corners = RoundedCorners(config.rounded_corners.value())
                           .sharp(TOP_RIGHT)
                           .sharp(BOTTOM_RIGHT);

  auto label = div(ctx, mk(entity, entity.id + 0),
                   ComponentConfig{
                       .size = config.size,
                       .label = original_label,
                       //
                       .color_usage = Theme::Usage::Primary,
                       //
                       .rounded_corners = label_corners,
                       // inheritables
                       .label_alignment = config.label_alignment,
                       .skip_when_tabbing = config.skip_when_tabbing,
                       .disabled = config.disabled,
                       .hidden = config.hidden,
                       // debugs
                       .debug_name = "slider_text",
                       .render_layer = config.render_layer + 0,
                   });
  label.ent()
      .template get<UIComponent>()
      .set_desired_width(config.size._scale_x(0.5f).x_axis)
      .set_desired_height(config.size.y_axis);

  auto elem_corners = RoundedCorners(config.rounded_corners.value())
                          .sharp(TOP_LEFT)
                          .sharp(BOTTOM_LEFT);

  auto elem = div(ctx, mk(entity, parent.id + entity.id + 0),
                  ComponentConfig{
                      .size = config.size,
                      .color_usage = Theme::Usage::Secondary,
                      .rounded_corners = elem_corners,
                      // inheritables
                      .label_alignment = config.label_alignment,
                      .skip_when_tabbing = config.skip_when_tabbing,
                      .disabled = config.disabled,
                      .hidden = config.hidden,
                      // debugs
                      .debug_name = "slider_background",
                      .render_layer = config.render_layer + 1,
                  });

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
          float v = (mouse_position.x - rect.x) / rect.width;
          if (v < mnf)
            v = mnf;
          if (v > mxf)
            v = mxf;

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
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

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

  config.size = ComponentSize(children(default_component_size.x),
                              pixels(default_component_size.y));
  config.flex_direction = FlexDirection::Row;

  std::string label_str = config.label;
  config.label = "";

  bool first_time = _init_component(ctx, entity, parent, config, "pagination");

  int child_index = 0;

  if (button(ctx, mk(entity),
             ComponentConfig{
                 .size = ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis},
                 .label = "<",
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "left",
                 .render_layer = (config.render_layer),
             })) {
    if (option_index > 1) {
      on_option_click(entity, option_index - 1);
    } else {
      on_option_click(entity, options.size());
    }
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(ctx, mk(entity, child_index + i),
               ComponentConfig{
                   .size = ComponentSize{pixels(default_component_size.x / 2.f),
                                         config.size.y_axis},
                   .label = std::string(options[i]),
                   // inheritables
                   .label_alignment = config.label_alignment,
                   .skip_when_tabbing = config.skip_when_tabbing,
                   .disabled = config.disabled,
                   .hidden = config.hidden,
                   // debugs
                   .debug_name = std::format("option {}", i + 1),
                   .render_layer = (config.render_layer + 1),
               })) {
      on_option_click(entity, i + 1);
    }
  }

  if (button(ctx, mk(entity),
             ComponentConfig{
                 .size = ComponentSize{pixels(default_component_size.x / 4.f),
                                       config.size.y_axis},
                 .label = ">",
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "right",
                 .render_layer = (config.render_layer),
             })) {
    if (option_index < options.size()) {
      on_option_click(entity, option_index + 1);
    } else {
      on_option_click(entity, 1);
    }
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
  Entity &entity = ep_pair.first;
  Entity &parent = ep_pair.second;

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

  config.size = ComponentSize(children(default_component_size.x),
                              pixels(default_component_size.y));

  std::string label_str = config.label;
  config.label = "";

  config = _overwrite_defaults(ctx, config);
  _init_component(ctx, entity, parent, config, "dropdown");

  auto size = ComponentSize{
      pixels(label_str.empty() ? default_component_size.x
                               : (default_component_size.x / 2.f)),
      pixels(default_component_size.y),
  };
  auto button_corners = std::bitset<4>(config.rounded_corners.value());

  if (!label_str.empty()) {
    auto label_corners = RoundedCorners(config.rounded_corners.value())
                             .sharp(TOP_RIGHT)
                             .sharp(BOTTOM_RIGHT);
    button_corners =
        RoundedCorners(button_corners).sharp(TOP_LEFT).sharp(BOTTOM_LEFT);

    auto label = div(ctx, mk(entity),
                     ComponentConfig{
                         .size = size,
                         .label = std::string(label_str),
                         .color_usage = Theme::Usage::Primary,
                         .rounded_corners = label_corners,
                         // inheritables
                         .label_alignment = config.label_alignment,
                         .skip_when_tabbing = config.skip_when_tabbing,
                         .disabled = config.disabled,
                         .hidden = config.hidden,
                         // debugs
                         .debug_name = "dropdown_label",
                         .render_layer = (config.render_layer + 0),
                         //
                     });
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
             ComponentConfig{
                 .size = size,
                 .label = main_button_label,
                 .rounded_corners = button_corners,
                 // inheritables
                 .label_alignment = config.label_alignment,
                 .skip_when_tabbing = config.skip_when_tabbing,
                 .disabled = config.disabled,
                 .hidden = config.hidden,
                 // debugs
                 .debug_name = "option 1",
                 .render_layer =
                     (config.render_layer + (dropdownState.on ? 0 : 0)),
             })) {

    dropdownState.on ? on_option_click(entity, 0) : toggle_visibility(entity);
  }

  if (auto result =
          button_group(ctx, mk(entity), options,
                       ComponentConfig{
                           // inheritables
                           .label_alignment = config.label_alignment,
                           .skip_when_tabbing = config.skip_when_tabbing,
                           .disabled = config.disabled,
                           .hidden = config.hidden || !dropdownState.on,
                           // debugs
                           .debug_name = std::format("dropdown button group"),
                           .render_layer = config.render_layer + 1,
                       });
      result) {
    on_option_click(entity, result.template as<int>());
  }

  option_index = dropdownState.last_option_clicked;
  return ElementResult{dropdownState.changed_since, entity,
                       dropdownState.last_option_clicked};
}

} // namespace imm

} // namespace ui

} // namespace afterhours