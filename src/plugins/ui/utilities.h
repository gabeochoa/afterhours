#pragma once

#include <format>
#include <iostream>
#include <string>
#if __has_include(<magic_enum/magic_enum.hpp>)
#include <magic_enum/magic_enum.hpp>
#else
#include "../../../vendor/magic_enum/magic_enum.hpp"
#endif

#include "../../developer.h"
#include "../../ecs.h"
#include "../../logging.h"
#include "../../core/text_cache.h"
#include "../../font_helper.h"
#include "../autolayout.h"
#include "../window_manager.h"
#include "components.h"
#include "context.h"

namespace afterhours {

namespace ui {

static inline void force_layout_and_print(
    Entity &root,
    window_manager::Resolution resolution = window_manager::Resolution()) {
  std::map<EntityID, RefEntity> components;
  auto comps = EntityQuery().whereHasComponent<ui::UIComponent>().gen();
  for (Entity &entity : comps) {
    components.emplace(entity.id, entity);
  }

  if (resolution.width == 0 || resolution.height == 0) {
    Entity &e =
        EntityQuery()
            .whereHasComponent<window_manager::ProvidesCurrentResolution>()
            .gen_first_enforce();
    resolution =
        e.get<window_manager::ProvidesCurrentResolution>().current_resolution;
  }

  ui::AutoLayout::autolayout(root.get<ui::UIComponent>(), resolution,
                             components);
  print_debug_autolayout_tree(root, root.get<ui::UIComponent>());
}

enum struct InputValidationMode { None, LogOnly, Assert };

constexpr static InputValidationMode validation_mode =
#if defined(AFTER_HOURS_INPUT_VALIDATION_ASSERT)
    InputValidationMode::Assert;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_LOG_ONLY)
    InputValidationMode::LogOnly;
#elif defined(AFTER_HOURS_INPUT_VALIDATION_NONE)
    InputValidationMode::None;
#else
    InputValidationMode::LogOnly;
#endif

// NOTE: i tried to write this as a constexpr function but
// the string joining wasnt working for me for some reason
#define validate_enum_has_value(enum_name, name, reason)                       \
  do {                                                                         \
    if constexpr (validation_mode == InputValidationMode::None) {              \
      log_error("validation mode none");                                       \
      return;                                                                  \
    }                                                                          \
                                                                               \
    if constexpr (!magic_enum::enum_contains<enum_name>(name)) {               \
      if constexpr (validation_mode == InputValidationMode::Assert) {          \
        static_assert(false, "InputAction missing value '" name                \
                             "'. Input used to " reason);                      \
      } else if constexpr (validation_mode == InputValidationMode::LogOnly) {  \
        log_warn("InputAction missing value '" name                            \
                 "'. Input used to " reason);                                  \
      } else {                                                                 \
      }                                                                        \
    }                                                                          \
  } while (0);

template <typename InputAction>
static void add_singleton_components(Entity &entity) {
  entity.addComponent<UIContext<InputAction>>();
  EntityHelper::registerSingleton<UIContext<InputAction>>(entity);

  entity.addComponent<ui::FontManager>()
      .load_font(UIComponent::DEFAULT_FONT, get_default_font())
      .load_font(UIComponent::SYMBOL_FONT, get_default_font())
      .load_font(UIComponent::UNSET_FONT, get_unset_font());
  EntityHelper::registerSingleton<ui::FontManager>(entity);

  auto &text_cache = entity.addComponent<ui::TextMeasureCache>();
  text_cache.set_measure_function(
      [](std::string_view text, std::string_view font_name, float font_size,
         float spacing) {
        auto font_manager = EntityHelper::get_singleton_cmp<ui::FontManager>();
        if (!font_manager) {
          return Vector2Type{0.0f, 0.0f};
        }
        const std::string font_name_str(font_name);
        const std::string text_str(text);
        Font font = font_manager->get_font(font_name_str);
        return measure_text(font, text_str.c_str(), font_size, spacing);
      });
  EntityHelper::registerSingleton<ui::TextMeasureCache>(entity);
}

template <typename InputAction>
static void enforce_singletons(SystemManager &sm) {
  validate_enum_has_value(InputAction, "None", "any unmapped input");
  validate_enum_has_value(InputAction, "WidgetMod",
                          "while held, press WidgetNext to do WidgetBack");
  validate_enum_has_value(InputAction, "WidgetNext",
                          "'tab' forward between ui elements");
  validate_enum_has_value(InputAction, "WidgetBack",
                          "'tab' back between ui elements");
  validate_enum_has_value(InputAction, "WidgetPress", "click on element");

  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<UIContext<InputAction>>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ui::FontManager>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ui::TextMeasureCache>>());
}

template <typename InputAction>
static void register_before_ui_updates(SystemManager &sm) {
  sm.register_update_system(std::make_unique<ui::ClearUIComponentChildren>());
  sm.register_update_system(
      std::make_unique<ui::BeginUIContextManager<InputAction>>());
}

template <typename InputAction>
static void register_after_ui_updates(SystemManager &sm) {
  {
    sm.register_update_system(
        std::make_unique<ui::UpdateDropdownOptions<InputAction>>());

    //
    sm.register_update_system(std::make_unique<ui::ClearVisibity>());
    sm.register_update_system(std::make_unique<ui::RunAutoLayout>());
    sm.register_update_system(
        std::make_unique<ui::TrackIfComponentWillBeRendered<InputAction>>());
    //
    sm.register_update_system(
        std::make_unique<ui::HandleTabbing<InputAction>>());
    sm.register_update_system(
        std::make_unique<ui::HandleClicks<InputAction>>());
    sm.register_update_system(
        std::make_unique<ui::CloseDropdownOnClickOutside<InputAction>>());
    sm.register_update_system(std::make_unique<ui::HandleDrags<InputAction>>());
    sm.register_update_system(
        std::make_unique<ui::HandleLeftRight<InputAction>>());
    sm.register_update_system(
        std::make_unique<ui::HandleSelectOnFocus<InputAction>>());
    // compute visual focus after input handlers adjusted logical focus
    sm.register_update_system(
        std::make_unique<ui::ComputeVisualFocusId<InputAction>>());
  }
  sm.register_update_system(
      std::make_unique<ui::EndUIContextManager<InputAction>>());
}

template <typename InputAction>
static void
register_render_systems(SystemManager &sm,
                        InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(std::make_unique<ui::RenderImm<InputAction>>());
  sm.register_render_system(
      std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
          toggle_debug));
}

template <typename InputAction>
static void
register_batched_render_systems(SystemManager &sm,
                                InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(std::make_unique<ui::RenderBatched<InputAction>>());
  sm.register_render_system(
      std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
          toggle_debug));
}

} // namespace ui

} // namespace afterhours
