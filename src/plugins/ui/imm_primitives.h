#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <string>
#include <utility>

#include "../../ecs.h"
#include "overlay.h"
#include "../input_system.h"
#include "component_init.h"
#include "components.h"
#include "element_result.h"
#include "measure_config.h"
#include "entity_management.h"
#include "fmt/format.h"
#include "rendering.h"
#include "rounded_corners.h"

namespace afterhours {

namespace ui {

namespace imm {

namespace detail {

inline size_t prev_index(size_t current, size_t total) {
  return (current == 0) ? total - 1 : current - 1;
}

inline size_t next_index(size_t current, size_t total) {
  return (current + 1) % total;
}

} // namespace detail

// ============================================================================
// imm::primitive — Stateless building blocks
//
// These take mutable references and flip/update them on interaction.
// No ECS state components are created. Caller owns all state.
// Signature convention: (ctx, ep_pair, config, &mutable_state...)
//
// TODO: Consider moving existing primitives (div, button, sprite, image)
//       into this namespace (breaking change for existing users).
// TODO: Consider whether stateful convenience wrappers should live in
//       imm::stateful namespace long-term.
// ============================================================================
namespace primitive {

/// A button that flips a bool on click. Stateless — caller owns the bool.
/// Returns ElementResult where bool() == true when value changed this frame.
///
/// Usage:
/// ```cpp
/// bool my_value = false;
/// if (primitive::toggle_button(ctx, mk(parent),
///         ComponentConfig{}.with_label(my_value ? "ON" : "OFF"),
///         my_value)) {
///     // value was flipped this frame
/// }
/// ```
ElementResult toggle_button(HasUIContext auto &ctx, EntityParent ep_pair,
                            ComponentConfig config, bool &value) {
  auto [entity, parent] = deref(ep_pair);

  // Outline & Ghost: transparent bg, readable text; Outline also adds a border
  if (config.button_variant != ButtonVariant::Filled) {
    Color original_bg = config.resolve_background_color(ctx.theme);
    config.with_custom_background(colors::transparent())
        .with_auto_text_color(false)
        .with_custom_text_color(ctx.theme.font);

    if (config.button_variant == ButtonVariant::Outline &&
        !config.has_border()) {
      config.with_border(original_bg,
                         h720(2.0f));
    }
  }

  init_component(ctx, ep_pair, config, ComponentType::Button, true,
                 "toggle_button");

  entity.get<UIComponent>().flex_direction = config.flex_direction;

  if (config.disabled) {
    entity.removeComponentIfExists<HasClickListener>();
    return ElementResult{false, entity, value};
  }

  entity.addComponentIfMissing<HasClickListener>([](Entity &) {});

  bool clicked = entity.get<HasClickListener>().down;
  if (clicked) {
    value = !value;
  }

  return ElementResult{clicked, entity, value};
}

} // namespace primitive

ElementResult div(HasUIContext auto &ctx, EntityParent ep_pair,
                  ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  if (config.size.is_default && config.label.empty())
    config.with_size(ComponentSize{children(), children()});
  if (config.size.is_default && !config.label.empty())
    config.with_size(ComponentSize{children(default_component_size.x),
                                   children(default_component_size.y)});

  init_component(ctx, ep_pair, config, ComponentType::Div);

  return {true, entity};
}


ElementResult image(HasUIContext auto &ctx, EntityParent ep_pair,
                    ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  init_component(ctx, ep_pair, config, ComponentType::Image, false, "image");

  return {false, entity};
}

ElementResult sprite(HasUIContext auto &ctx, EntityParent ep_pair,
                     afterhours::texture_manager::Texture texture,
                     ComponentConfig config = ComponentConfig()) {
  afterhours::texture_manager::Rectangle full_rect{
      0, 0, static_cast<float>(texture.width),
      static_cast<float>(texture.height)};
  return sprite(ctx, ep_pair, texture, full_rect, config);
}

ElementResult sprite(HasUIContext auto &ctx, EntityParent ep_pair,
                     afterhours::texture_manager::Texture texture,
                     afterhours::texture_manager::Rectangle source_rect,
                     ComponentConfig config = ComponentConfig()) {
  auto [entity, parent] = deref(ep_pair);

  init_component(ctx, ep_pair, config, ComponentType::Image, false, "sprite");

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

  init_component(ctx, ep_pair, config, ComponentType::Image, true,
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


} // namespace imm

} // namespace ui

} // namespace afterhours
