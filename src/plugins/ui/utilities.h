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
#include "ui_collection.h"

namespace afterhours {

namespace ui {

static inline void force_layout_and_print(
    Entity &root,
    window_manager::Resolution resolution = window_manager::Resolution()) {
  std::map<EntityID, RefEntity> components;
  auto &ui_coll = UICollectionHolder::get().collection;
  ui_coll.merge_entity_arrays();
  auto comps = EntityQuery(ui_coll, {.ignore_temp_warning = true})
                   .whereHasComponent<ui::UIComponent>()
                   .gen();
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

// Initialize the UI plugin. Creates the root UI entity and all singletons
// in the UI collection. Singletons are also registered in the default
// collection so external code (toast, modal, game code) can find them.
// Returns a reference to the root entity.
template <typename InputAction>
static Entity &init_ui_plugin() {
  auto &ui_coll = UICollectionHolder::get().collection;
  Entity &ui_root = ui_coll.createPermanentEntity();
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  // Grab the shared_ptr before anything else merges temp_entities
  auto root_shared = ui_coll.temp_entities.back();
#endif

  // UIContext
  ui_root.addComponent<UIContext<InputAction>>();
  ui_coll.registerSingleton<UIContext<InputAction>>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  EntityHelper::registerSingleton<UIContext<InputAction>>(ui_root);
#endif

  // FontManager
  ui_root.addComponent<ui::FontManager>()
      .load_font(UIComponent::DEFAULT_FONT, get_default_font())
      .load_font(UIComponent::SYMBOL_FONT, get_default_font())
      .load_font(UIComponent::UNSET_FONT, get_unset_font());
  ui_coll.registerSingleton<ui::FontManager>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  EntityHelper::registerSingleton<ui::FontManager>(ui_root);
#endif

  // TextMeasureCache
  auto &text_cache = ui_root.addComponent<ui::TextMeasureCache>();
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
  ui_coll.registerSingleton<ui::TextMeasureCache>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  EntityHelper::registerSingleton<ui::TextMeasureCache>(ui_root);
#endif

  // UIEntityMappingCache
  ui_root.addComponent<ui::UIEntityMappingCache>();
  ui_coll.registerSingleton<ui::UIEntityMappingCache>(ui_root);
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  EntityHelper::registerSingleton<ui::UIEntityMappingCache>(ui_root);
#endif

  // Root UI component
  ui_root.addComponent<ui::AutoLayoutRoot>();
  ui_root.addComponent<ui::UIComponentDebug>("ui_root");
  ui_root.addComponent<ui::UIComponent>(ui_root.id)
      .set_desired_width(ui::screen_pct(1.f))
      .set_desired_height(ui::screen_pct(1.f))
      .enable_font(ui::UIComponent::DEFAULT_FONT,
                   afterhours::ui::pixels(75.f));

  // Validate InputAction enum
  validate_enum_has_value(InputAction, "None", "any unmapped input");
  validate_enum_has_value(InputAction, "WidgetMod",
                          "while held, press WidgetNext to do WidgetBack");
  validate_enum_has_value(InputAction, "WidgetNext",
                          "'tab' forward between ui elements");
  validate_enum_has_value(InputAction, "WidgetBack",
                          "'tab' back between ui elements");
  validate_enum_has_value(InputAction, "WidgetPress", "click on element");

  // In split-collection mode, also add root entity to default collection so
  // that screen systems (which iterate default collection entities via
  // for_each_with) can find it. In single-collection mode this is a no-op
  // since ui_coll IS the default collection.
#ifndef AFTER_HOURS_UI_SINGLE_COLLECTION
  auto &default_coll = EntityHelper::get_default_collection();
  default_coll.temp_entities.push_back(root_shared);
  default_coll.permanant_ids.insert(ui_root.id);
#endif

  return ui_root;
}

// Helper: run a list of systems on UI collection entities
inline void run_systems_on_ui_entities(
    std::vector<std::unique_ptr<SystemBase>> &systems, float dt,
    bool is_render = false) {
  auto &ui_coll = UICollectionHolder::get().collection;
  ui_coll.merge_entity_arrays();

  for (auto &system : systems) {
    if (!system->should_run(dt))
      continue;

    auto &entities = ui_coll.get_entities_for_mod();

    system->once(dt);
    system->on_iteration_begin(dt);
    for (auto &entity : entities) {
      if (!entity)
        continue;
      if (system->include_derived_children)
        system->for_each_derived(*entity, dt);
      else
        system->for_each(*entity, dt);
    }
    system->on_iteration_end(dt);
    system->after(dt);

    if (is_render) {
      const SystemBase &csys = *system;
      csys.once(dt);
      csys.on_iteration_begin(dt);
      for (auto &entity : entities) {
        if (!entity)
          continue;
        const Entity &e = *entity;
        if (csys.include_derived_children)
          csys.for_each_derived(e, dt);
        else
          csys.for_each(e, dt);
      }
      csys.on_iteration_end(dt);
      csys.after(dt);
    }

    ui_coll.merge_entity_arrays();
  }
}

// Bridge system: runs ClearUIComponentChildren + BeginUIContextManager
// on UI collection entities.
template <typename InputAction>
struct UIPluginPreUpdateBridge : System<> {
  std::vector<std::unique_ptr<SystemBase>> systems;

  UIPluginPreUpdateBridge() {
    systems.push_back(
        std::make_unique<ui::ClearUIComponentChildren>());
    systems.push_back(
        std::make_unique<ui::BeginUIContextManager<InputAction>>());
  }

  virtual void once(float dt) override {
    run_systems_on_ui_entities(systems, dt);
  }
};

// Bridge system: runs all post-user-code UI update systems on UI collection
// entities.
template <typename InputAction>
struct UIPluginPostUpdateBridge : System<> {
  std::vector<std::unique_ptr<SystemBase>> systems;

  UIPluginPostUpdateBridge() {
    systems.push_back(
        std::make_unique<ui::UpdateDropdownOptions<InputAction>>());
    systems.push_back(std::make_unique<ui::ClearVisibity>());
    systems.push_back(std::make_unique<ui::BuildUIEntityMapping>());
    systems.push_back(std::make_unique<ui::RunAutoLayout>());
    systems.push_back(
        std::make_unique<ui::TrackIfComponentWillBeRendered<InputAction>>());
    systems.push_back(
        std::make_unique<ui::HandleTabbing<InputAction>>());
    systems.push_back(
        std::make_unique<ui::HandleClicks<InputAction>>());
    systems.push_back(
        std::make_unique<
            ui::CloseDropdownOnClickOutside<InputAction>>());
    systems.push_back(
        std::make_unique<ui::HandleDrags<InputAction>>());
    systems.push_back(
        std::make_unique<ui::HandleLeftRight<InputAction>>());
    systems.push_back(
        std::make_unique<ui::HandleSelectOnFocus<InputAction>>());
    systems.push_back(
        std::make_unique<ui::ComputeVisualFocusId<InputAction>>());
    systems.push_back(
        std::make_unique<ui::EndUIContextManager<InputAction>>());
  }

  virtual void once(float dt) override {
    run_systems_on_ui_entities(systems, dt);
    UICollectionHolder::get().collection.cleanup();
  }
};

// Bridge system: runs UI render systems on UI collection entities.
template <typename InputAction>
struct UIPluginRenderBridge : System<> {
  std::vector<std::unique_ptr<SystemBase>> systems;

  UIPluginRenderBridge(InputAction toggle_debug, bool use_batched) {
    if (use_batched) {
      systems.push_back(
          std::make_unique<ui::RenderBatched<InputAction>>());
    } else {
      systems.push_back(
          std::make_unique<ui::RenderImm<InputAction>>());
    }
    systems.push_back(
        std::make_unique<ui::RenderDebugAutoLayoutRoots<InputAction>>(
            toggle_debug));
  }

  virtual void once(float dt) override {
    run_systems_on_ui_entities(systems, dt, /*is_render=*/true);
  }
};

// --- Registration functions ---

template <typename InputAction>
static void enforce_singletons(SystemManager &) {
  // Compile-time validation only. UI singletons live in the UI collection
  // and are accessed via bridge systems, so EnforceSingleton systems
  // (which iterate default collection) are not needed.
  validate_enum_has_value(InputAction, "None", "any unmapped input");
  validate_enum_has_value(InputAction, "WidgetMod",
                          "while held, press WidgetNext to do WidgetBack");
  validate_enum_has_value(InputAction, "WidgetNext",
                          "'tab' forward between ui elements");
  validate_enum_has_value(InputAction, "WidgetBack",
                          "'tab' back between ui elements");
  validate_enum_has_value(InputAction, "WidgetPress", "click on element");
}

template <typename InputAction>
static void register_before_ui_updates(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<ui::UIPluginPreUpdateBridge<InputAction>>());
}

template <typename InputAction>
static void register_after_ui_updates(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<ui::UIPluginPostUpdateBridge<InputAction>>());
}

template <typename InputAction>
static void
register_render_systems(SystemManager &sm,
                        InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(
      std::make_unique<ui::UIPluginRenderBridge<InputAction>>(
          toggle_debug, /*use_batched=*/false));
}

template <typename InputAction>
static void
register_batched_render_systems(SystemManager &sm,
                                InputAction toggle_debug = InputAction::None) {
  sm.register_render_system(
      std::make_unique<ui::UIPluginRenderBridge<InputAction>>(
          toggle_debug, /*use_batched=*/true));
}

} // namespace ui

} // namespace afterhours
