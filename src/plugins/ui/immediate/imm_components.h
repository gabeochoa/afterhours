#pragma once

#include "fmt/format.h"
#include <algorithm>
#include <array>
#include <bitset>
#include <string>

#include "../../../ecs.h"
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

  _init_component(ctx, ep_pair, config, ComponentType::Image, false, "image");

  return {false, entity};
}

ElementResult sprite(HasUIContext auto &ctx, EntityParent ep_pair,
                     afterhours::texture_manager::Texture texture,
                     afterhours::texture_manager::Rectangle source_rect,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image, false, "sprite");

  auto alignment = config.image_alignment.value_or(
      afterhours::texture_manager::HasTexture::Alignment::Center);
  auto &img = entity.addComponentIfMissing<ui::HasImage>(texture, source_rect,
                                                         alignment);
  img.texture = texture;
  img.source_rect = source_rect;
  img.alignment = alignment;

  return {false, entity};
}

inline ElementResult
image_button(HasUIContext auto &ctx, EntityParent ep_pair,
             afterhours::texture_manager::Texture texture,
             afterhours::texture_manager::Rectangle source_rect,
             ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Image, true,
                  "image_button");

  auto alignment = config.image_alignment.value_or(
      afterhours::texture_manager::HasTexture::Alignment::Center);
  auto &img = entity.addComponentIfMissing<ui::HasImage>(texture, source_rect,
                                                         alignment);
  img.texture = texture;
  img.source_rect = source_rect;
  img.alignment = alignment;

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});
  return ElementResult{entity.get<HasClickListener>().down, entity};
}

template <typename Container>
ElementResult icon_row(HasUIContext auto &ctx, EntityParent ep_pair,
                       afterhours::texture_manager::Texture spritesheet,
                       const Container &frames, float scale = 1.f,
                       ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  auto row =
      div(ctx, ep_pair,
          ComponentConfig::inherit_from(config, "icon_row")
              .with_size(config.size)
              .with_margin(config.margin)
              .with_padding(config.padding)
              .with_flex_direction(FlexDirection::Row)
              .with_debug_name(config.debug_name.empty() ? "icon_row"
                                                         : config.debug_name));

  size_t i = 0;
  for (const auto &frame : frames) {
    auto icon_width = pixels(frame.width * scale);
    auto icon_height = pixels(frame.height * scale);

    sprite(ctx, mk(row.ent(), static_cast<int>(i)), spritesheet, frame,
           ComponentConfig::inherit_from(config)
               .with_image_alignment(
                   afterhours::texture_manager::HasTexture::Alignment::Center)
               .with_size(ComponentSize{icon_width, icon_height})
               .with_debug_name(fmt::format("icon_row_item_{}", i)));
    i++;
  }

  return {false, row.ent()};
}

ElementResult button(HasUIContext auto &ctx, EntityParent ep_pair,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  _init_component(ctx, ep_pair, config, ComponentType::Button, true, "button");

  // Apply flex-direction specifically for buttons so they can drive wrapping
  // TODO: this is a hack to get buttons to wrap. We should find a better way to
  // do this.
  entity.get<UIComponent>().flex_direction = config.flex_direction;

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
                                          fmt::format("button group {}", i))
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
  config.font_name = UIComponent::SYMBOL_FONT;
  config.font_size = 16.f;

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

  // 2025-08-11: ensure checkbox row uses responsive defaults so both label and
  // button scale with resolution. Previously, only the label used a responsive
  // size, causing the button to remain tiny at higher DPIs/resolutions.
  {
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def =
            styling_defaults.get_component_config(ComponentType::Checkbox);
        def.has_value()) {
      config.size = def->size;
    } else {
      config.size = ComponentSize(pixels(default_component_size.x),
                                  children(default_component_size.y));
    }
  }
  bool has_label_child = !label.empty();
  if (has_label_child) {
    config.size = config.size._scale_x(0.5f);

    auto label_config =
        ComponentConfig::inherit_from(
            config, fmt::format("checkbox label {}", config.debug_name))
            .with_size(config.size)
            .with_label(label);

    // TODO - if the user wants to mess with the corners, how can we merge these
    if (config.color_usage == Theme::Usage::Default)
      label_config.with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(RoundedCorners().right_sharp());

    auto label_ent = div(ctx, mk(entity), label_config);
    label_ent.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // 2025-08-11: explicitly propagate the responsive size to the clickable
  // checkbox so it scales along with the label and row container.
  auto checkbox_config =
      ComponentConfig::inherit_from(
          config, fmt::format("checkbox indiv from {}", config.debug_name))
          .with_size(config.size);

  if (config.color_usage == Theme::Usage::Default)
    checkbox_config.with_color_usage(Theme::Usage::Primary)
        .with_rounded_corners(RoundedCorners().left_sharp());

  bool changed = false;
  auto checkbox_ent =
      checkbox_no_label(ctx, mk(entity), value, checkbox_config);
  checkbox_ent.ent().template addComponentIfMissing<InFocusCluster>();
  if (checkbox_ent) {
    changed = true;
  }

  // Mark the container row as the focus cluster root so focus ring surrounds
  // both the label (if present) and the checkbox box.
  entity.template addComponentIfMissing<FocusClusterRoot>();

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
                                          fmt::format("checkbox row {}", i))
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

// Helper function to generate label text based on position and value
static std::string
generate_label_text(const std::string &original_label, float value,
                    SliderHandleValueLabelPosition position) {
  switch (position) {
  case SliderHandleValueLabelPosition::None:
  case SliderHandleValueLabelPosition::OnHandle:
    return original_label;
  case SliderHandleValueLabelPosition::WithLabel:
    return original_label + ": " +
           std::to_string(static_cast<int>(value * 100)) + "%";
  case SliderHandleValueLabelPosition::WithLabelNewLine:
    return original_label + "\n" +
           std::to_string(static_cast<int>(value * 100)) + "%";
  }
  return original_label;
}

// Helper function to update label entity
static void update_label_entity(Entity &entity, const std::string &new_text) {
  if (entity.has<ui::HasLabel>()) {
    entity.get<ui::HasLabel>().set_label(new_text);
  }
}

// Helper function to find and update handle label
static void update_handle_label(Entity &handle_entity, float value) {
  UIComponent &handle_cmp = handle_entity.get<UIComponent>();
  for (EntityID child_id : handle_cmp.children) {
    Entity &child_entity = EntityHelper::getEntityForIDEnforce(child_id);
    if (child_entity.has<ui::HasLabel>()) {
      update_label_entity(child_entity,
                          std::to_string(static_cast<int>(value * 100)));
      break;
    }
  }
}

// Helper function to find and update main label
static void update_main_label(Entity &slider_entity,
                              const std::string &original_label, float value,
                              SliderHandleValueLabelPosition position) {
  UIComponent &slider_cmp = slider_entity.get<UIComponent>();
  if (!slider_cmp.children.empty()) {
    EntityID main_label_id = slider_cmp.children[0];
    Entity &main_label_entity =
        EntityHelper::getEntityForIDEnforce(main_label_id);
    std::string new_text = generate_label_text(original_label, value, position);
    update_label_entity(main_label_entity, new_text);
  }
}

ElementResult slider(HasUIContext auto &ctx, EntityParent ep_pair,
                     float &owned_value,
                     ComponentConfig config = ComponentConfig(),
                     SliderHandleValueLabelPosition handle_label_position =
                         SliderHandleValueLabelPosition::None) {
  auto [entity, parent] = deref(ep_pair);

  std::string original_label = config.label;
  config.label = "";

  auto original_color_usage = config.color_usage;
  config.with_color_usage(Theme::Usage::None);
  _init_component(ctx, ep_pair, config, ComponentType::Slider, true, "slider");
  config.color_usage = original_color_usage;

  // Create main label
  std::string main_label_text =
      generate_label_text(original_label, owned_value, handle_label_position);
  auto label_corners = RoundedCorners(config.rounded_corners.value())
                           .sharp(TOP_RIGHT)
                           .sharp(BOTTOM_RIGHT);

  auto label = div(ctx, mk(entity, entity.id + 0),
                   ComponentConfig::inherit_from(config, "slider_text")
                       .with_size(config.size)
                       .with_label(main_label_text)
                       .with_color_usage(Theme::Usage::Primary)
                       .with_rounded_corners(label_corners)
                       .with_render_layer(config.render_layer + 0));
  label.ent()
      .template get<UIComponent>()
      .set_desired_width(config.size._scale_x(0.5f).x_axis)
      .set_desired_height(config.size.y_axis);
  label.ent().template addComponentIfMissing<InFocusCluster>();

  // Create slider background
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
  slider_bg.template addComponentIfMissing<InFocusCluster>();

  if (slider_bg.is_missing<ui::HasSliderState>())
    slider_bg.addComponent<ui::HasSliderState>(owned_value);

  HasSliderState &sliderState = slider_bg.get<ui::HasSliderState>();
  sliderState.changed_since = true;

  // Create value update function
  auto apply_slider_value = [&](Entity &target, float new_value_pct) {
    float clamped = std::clamp(new_value_pct, 0.f, 1.f);
    if (clamped == sliderState.value)
      return;
    sliderState.value = clamped;
    sliderState.changed_since = true;

    UIComponent &cmp = target.get<UIComponent>();
    Rectangle rect = cmp.rect();
    if (!cmp.children.empty()) {
      EntityID child_id = cmp.children[0];
      Entity &child = EntityHelper::getEntityForIDEnforce(child_id);
      UIComponent &child_cmp = child.get<UIComponent>();
      child_cmp.set_desired_padding(
          pixels(sliderState.value * 0.75f * rect.width), Axis::left);

      // Update labels based on position
      if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
        update_handle_label(child, sliderState.value);
      } else if (handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabel ||
                 handle_label_position ==
                     SliderHandleValueLabelPosition::WithLabelNewLine) {
        update_main_label(target, original_label, sliderState.value,
                          handle_label_position);
      }
    }
  };

  // Add drag listener
  slider_bg.addComponentIfMissing<ui::HasDragListener>(
      [apply_slider_value](Entity &draggable) {
        UIComponent &cmp = draggable.get<UIComponent>();
        Rectangle rect = cmp.rect();
        auto mouse_position = input::get_mouse_position();
        float v = (mouse_position.x - rect.x) / rect.width;
        apply_slider_value(draggable, v);
      });

  // Create handle
  const auto dim = config.size.x_axis.dim;
  const float width_val = config.size.x_axis.value;

  // Warn about tiny widths
  const bool tiny_width =
      (dim == Dim::Pixels && width_val < 8.0f) ||
      ((dim == Dim::Percent || dim == Dim::ScreenPercent) && width_val < 0.02f);
  if (tiny_width) {
    log_warn("slider width is very small (dim={}, value={:.4f}); slider handle "
             "may be invisible (component: {})",
             (int)dim, width_val, config.debug_name.c_str());
  }

  Size handle_width_size{dim, width_val * 0.25f, config.size.x_axis.strictness};
  if (dim == Dim::Pixels)
    handle_width_size.value = std::max(2.0f, handle_width_size.value);
  else if (dim == Dim::Percent || dim == Dim::ScreenPercent)
    handle_width_size.value = std::max(0.02f, handle_width_size.value);

  Size handle_left_size{dim, owned_value * 0.75f * width_val,
                        config.size.x_axis.strictness};
  if (dim == Dim::Pixels)
    handle_left_size.value = std::max(0.0f, handle_left_size.value);
  else if (dim == Dim::Percent || dim == Dim::ScreenPercent)
    handle_left_size.value = std::max(0.0f, handle_left_size.value);

  auto handle_config =
      ComponentConfig::inherit_from(config, "slider_handle")
          .with_size(ComponentSize{handle_width_size, config.size.y_axis})
          .with_padding(Padding{.left = handle_left_size})
          .with_color_usage(Theme::Usage::Primary)
          .with_rounded_corners(config.rounded_corners.value())
          .with_debug_name("slider_handle")
          .with_render_layer(config.render_layer + 2);

  auto handle = div(ctx, mk(slider_bg), handle_config);
  handle.cmp()
      .set_desired_width(handle_config.size.x_axis)
      .set_desired_height(config.size.y_axis);
  handle.ent().template addComponentIfMissing<InFocusCluster>();

  // Add handle label if needed
  if (handle_label_position == SliderHandleValueLabelPosition::OnHandle) {
    std::string handle_label_text =
        std::to_string(static_cast<int>(sliderState.value * 100));
    auto handle_label_config =
        ComponentConfig::inherit_from(config, "slider_handle_label")
            .with_label(handle_label_text)
            .with_size(ComponentSize{handle_width_size, config.size.y_axis})
            .with_color_usage(Theme::Usage::Primary)
            .with_render_layer(config.render_layer + 3)
            .with_font(config.font_name, config.font_size);

    auto handle_label = div(ctx, mk(handle.ent()), handle_label_config);
    handle_label.ent().template addComponentIfMissing<InFocusCluster>();
  }

  // Add keyboard listener
  slider_bg.addComponentIfMissing<ui::HasLeftRightListener>(
      [apply_slider_value, &sliderState](Entity &ent, int dir) {
        const float step = 0.01f;
        apply_slider_value(ent, sliderState.value + (dir < 0 ? -step : step));
      });

  owned_value = sliderState.value;
  entity.template addComponentIfMissing<FocusClusterRoot>();
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

  // Use styling defaults for size if none provided
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
              .with_font(UIComponent::SYMBOL_FONT, 16.f)
              .with_render_layer(config.render_layer))) {
    on_option_click(entity, prev_index(option_index - 1, options.size()));
  }

  for (size_t i = 0; i < options.size(); i++) {
    if (button(
            ctx, mk(entity, child_index + i),
            ComponentConfig::inherit_from(config,
                                          fmt::format("option {}", i + 1))
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
              .with_font(UIComponent::SYMBOL_FONT, 16.f)
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
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def =
            styling_defaults.get_component_config(ComponentType::Dropdown)) {
      config.size = (*def).size;
    } else {
      config.size = ComponentSize(children(default_component_size.x),
                                  pixels(default_component_size.y));
    }
  }

  std::string label_str = config.label;
  config.label = "";
  config.flex_direction = FlexDirection::Row;

  _init_component(ctx, ep_pair, config, ComponentType::Dropdown);

  auto button_corners =
      config.rounded_corners.value_or(ctx.theme.rounded_corners);

  auto config_size = config.size;

  bool has_label_child = !label_str.empty();
  if (has_label_child) {
    config_size = config.size._scale_x(0.5f);
    button_corners = RoundedCorners(button_corners).left_sharp();

    auto label = div(
        ctx, mk(entity),
        ComponentConfig::inherit_from(config, "dropdown_label")
            .with_size(config_size)
            .with_label(std::string(label_str))
            .with_color_usage(Theme::Usage::Primary)
            .with_rounded_corners(RoundedCorners(button_corners).right_sharp())
            .with_render_layer(config.render_layer + 0));
    label.ent().template addComponentIfMissing<InFocusCluster>();
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
  auto main_button_label = fmt::format("{}{}", current_option, drop_arrow_icon);
  // TODO hot sibling summary: previously, when a label was present to the left
  // of the dropdown button, we passed that label entity id as a "hot sibling"
  // to the main button so hovering/focusing the button would visually hot the
  // label too. Implementation details we removed:
  // - ComponentConfig had a std::vector<EntityID> hot_siblings with builder
  //   helpers with_hot_siblings/add_hot_sibling.
  // - Applying config added a ui::BringsHotSiblings component to the target
  //   entity, storing those ids.
  // - In rendering, when an entity became hot, we iterated its parent's
  //   children and, for each sibling entity that had BringsHotSiblings
  //   including the current entity id, we treated that sibling as hot as well.
  // - In this dropdown, when a label existed, we collected the label child id
  //   and passed it via with_hot_siblings({label_id}) to the main button.
  // Re-adding this would require restoring: ComponentConfig hot_siblings api,
  // ui::BringsHotSiblings component, and the rendering propagation logic.
  auto main_btn = button(
      ctx, mk(entity),
      ComponentConfig::inherit_from(config, "option 1")
          .with_size(config_size)
          .with_label(main_button_label)
          .with_rounded_corners(button_corners)
          // TODO This works great but we need a way to
          // close the dropdown when you leave without selecting anything
          //  .with_select_on_focus(true)
          .with_render_layer(config.render_layer + (dropdownState.on ? 0 : 0)));
  if (main_btn) {

    dropdownState.on ? on_option_click(entity, 0) : toggle_visibility(entity);
  }

  // Mark the label + main dropdown button as a focus cluster, but do not
  // include dropdown items (they should be separately focusable when open).
  entity.template addComponentIfMissing<FocusClusterRoot>();
  // Mark the main dropdown button as part of the cluster
  main_btn.ent().template addComponentIfMissing<InFocusCluster>();

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
    auto &styling_defaults = UIStylingDefaults::get();
    if (auto def = styling_defaults.get_component_config(
            ComponentType::NavigationBar)) {
      config.size = (*def).size;
    } else {
      config.size = ComponentSize(pixels(default_component_size.x),
                                  pixels(default_component_size.y));
    }
  }
  // TODO - add default
  config.flex_direction = FlexDirection::Row;

  // Prevent the parent navigation bar from getting a background color
  config.with_color_usage(Theme::Usage::None);
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
                 .with_font(UIComponent::SYMBOL_FONT, 16.f)
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
                 .with_font(UIComponent::SYMBOL_FONT, 16.f)
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
