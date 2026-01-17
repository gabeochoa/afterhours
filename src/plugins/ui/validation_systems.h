#pragma once

// ============================================================
// UI Validation Plugin
// ============================================================
//
// This is an optional plugin for enforcing design rules at runtime.
// Include this header and call register_validation_systems() to enable.
//
// Usage:
//   #include <afterhours/src/plugins/ui/validation_systems.h>
//
//   // Enable validation in development mode
//   UIStylingDefaults::get().enable_development_validation();
//
//   // Register validation systems after other UI systems
//   ui::validation::register_systems<InputAction>(system_manager);
//
//   // Or register render overlay for visual debugging
//   ui::validation::register_render_systems<InputAction>(system_manager);
//

#include <map>
#include <set>
#include <vector>

#include "../../ecs.h"
#include "../../logging.h"
#include "../autolayout.h"
#include "../window_manager.h"
#include "component_config.h"
#include "components.h"
#include "context.h"
#include "theme.h"
#include "validation_config.h"

namespace afterhours {
namespace ui {
namespace validation {

// ============================================================
// Validation Systems
// ============================================================

// Clears validation violations from previous frame
struct ClearViolations : System<ValidationViolation> {
  virtual void for_each_with(Entity &entity, ValidationViolation &,
                             float) override {
    entity.removeComponent<ValidationViolation>();
  }
};

// Validates that all UI components stay within screen safe area bounds
struct ValidateScreenBounds : System<AutoLayoutRoot, UIComponent> {
  window_manager::Resolution screen;

  virtual void once(float) override {
    Entity &e = EntityHelper::get_singleton<
        window_manager::ProvidesCurrentResolution>();
    screen =
        e.get<window_manager::ProvidesCurrentResolution>().current_resolution;
  }

  void validate_bounds(UIComponent &cmp, const ValidationConfig &config,
                       float margin) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    auto rect = cmp.rect();

    // Check if any edge is outside safe area
    bool left_violation = rect.x < margin;
    bool top_violation = rect.y < margin;
    bool right_violation = rect.x + rect.width > screen.width - margin;
    bool bottom_violation = rect.y + rect.height > screen.height - margin;

    if (left_violation || top_violation || right_violation ||
        bottom_violation) {
      std::string edges;
      if (left_violation)
        edges += "left ";
      if (top_violation)
        edges += "top ";
      if (right_violation)
        edges += "right ";
      if (bottom_violation)
        edges += "bottom ";

      std::string msg = "Component outside safe area (" + edges + ") at (" +
                        std::to_string(rect.x) + "," + std::to_string(rect.y) +
                        ") size " + std::to_string(rect.width) + "x" +
                        std::to_string(rect.height);

      report_violation(config, "ScreenBounds", msg, cmp.id);

      // Add violation component for visual debugging
      if (config.highlight_violations) {
        OptEntity opt_ent = EntityHelper::getEntityForID(cmp.id);
        if (opt_ent.valid()) {
          Entity &ent = opt_ent.asE();
          if (!ent.has<ValidationViolation>()) {
            ent.addComponent<ValidationViolation>(msg, "ScreenBounds", 1.0f);
          }
        }
      }
    }

    // Recursively check children
    for (EntityID child_id : cmp.children) {
      validate_bounds(AutoLayout::to_cmp_static(child_id), config, margin);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_screen_bounds)
      return;

    validate_bounds(cmp, config, config.safe_area_margin);
  }
};

// Validates that child components stay within their parent bounds
struct ValidateChildContainment : System<AutoLayoutRoot, UIComponent> {

  void validate_containment(UIComponent &cmp, const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    auto parent_rect = cmp.rect();

    for (EntityID child_id : cmp.children) {
      UIComponent &child = AutoLayout::to_cmp_static(child_id);

      if (child.should_hide)
        continue;

      auto child_rect = child.rect();

      // Check if child overflows parent on any edge
      bool left_overflow = child_rect.x < parent_rect.x;
      bool top_overflow = child_rect.y < parent_rect.y;
      bool right_overflow =
          child_rect.x + child_rect.width > parent_rect.x + parent_rect.width;
      bool bottom_overflow =
          child_rect.y + child_rect.height > parent_rect.y + parent_rect.height;

      if (left_overflow || top_overflow || right_overflow || bottom_overflow) {
        std::string edges;
        if (left_overflow)
          edges += "left ";
        if (top_overflow)
          edges += "top ";
        if (right_overflow)
          edges += "right ";
        if (bottom_overflow)
          edges += "bottom ";

        std::string msg = "Child overflows parent (" + edges + ") child at (" +
                          std::to_string(child_rect.x) + "," +
                          std::to_string(child_rect.y) + ") size " +
                          std::to_string(child_rect.width) + "x" +
                          std::to_string(child_rect.height) + ", parent at (" +
                          std::to_string(parent_rect.x) + "," +
                          std::to_string(parent_rect.y) + ") size " +
                          std::to_string(parent_rect.width) + "x" +
                          std::to_string(parent_rect.height);

        report_violation(config, "ChildContainment", msg, child.id, 0.8f);

        // Add violation component for visual debugging
        if (config.highlight_violations) {
          OptEntity opt_ent = EntityHelper::getEntityForID(child.id);
          if (opt_ent.valid()) {
            Entity &ent = opt_ent.asE();
            if (!ent.has<ValidationViolation>()) {
              ent.addComponent<ValidationViolation>(msg, "ChildContainment",
                                                    0.8f);
            }
          }
        }
      }

      // Recursively check this child's children
      validate_containment(child, config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_child_containment)
      return;

    validate_containment(cmp, config);
  }
};

// Validates contrast ratios between text and background colors
struct ValidateComponentContrast : System<UIComponent, HasColor, HasLabel> {
  virtual void for_each_with(Entity &entity, UIComponent &cmp, HasColor &bg,
                             HasLabel &label, float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_contrast_ratio)
      return;

    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    // Get the theme to determine text color
    auto &theme_defaults = imm::ThemeDefaults::get();
    Theme theme = theme_defaults.get_theme();

    Color bg_color = bg.color();
    Color text_color;

    // Check if auto_text_color is enabled (indicated by background_hint)
    if (label.background_hint.has_value()) {
      // auto_text_color picks the best contrast between font and darkfont
      text_color =
          colors::auto_text_color(bg_color, theme.font, theme.darkfont);
    } else if (label.explicit_text_color.has_value()) {
      // Explicit text color was set
      text_color = label.explicit_text_color.value();
    } else {
      // Default to theme font color
      text_color = theme.font;
    }

    float ratio = colors::contrast_ratio(text_color, bg_color);

    if (ratio < config.min_contrast_ratio) {
      std::string msg = "Contrast ratio " + std::to_string(ratio) +
                        " below minimum " +
                        std::to_string(config.min_contrast_ratio) +
                        " for text: \"" + label.label + "\"";

      report_violation(config, "ContrastRatio", msg, entity.id, 0.9f);

      // Add violation component for visual debugging
      if (config.highlight_violations) {
        if (!entity.has<ValidationViolation>()) {
          entity.addComponent<ValidationViolation>(msg, "ContrastRatio", 0.9f);
        }
      }
    }
  }
};

// Validates minimum font size for readability
struct ValidateMinFontSize : System<UIComponent, HasLabel> {
  virtual void for_each_with(Entity &entity, UIComponent &cmp, HasLabel &,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_min_font_size)
      return;

    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    float font_size = cmp.font_size;

    if (font_size < config.min_font_size) {
      std::string msg = "Font size " + std::to_string(font_size) +
                        "px below minimum " +
                        std::to_string(config.min_font_size) + "px";

      report_violation(config, "MinFontSize", msg, entity.id, 0.6f);

      // Add violation component for visual debugging
      if (config.highlight_violations) {
        if (!entity.has<ValidationViolation>()) {
          entity.addComponent<ValidationViolation>(msg, "MinFontSize", 0.6f);
        }
      }
    }
  }
};

// ============================================================
// Validation Render Overlay
// ============================================================

// Renders visual indicators for validation violations
// Should be registered after RenderImm to draw on top
template <typename InputAction>
struct RenderOverlay : System<UIContext<InputAction>> {
  RenderOverlay() : System<UIContext<InputAction>>() {}

  virtual void for_each_with(Entity &, UIContext<InputAction> &,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    // Only render if highlight_violations is enabled
    if (!config.highlight_violations)
      return;

    // Find all entities with validation violations
    auto violations =
        EntityQuery().whereHasComponent<ValidationViolation>().gen();

    for (Entity &entity : violations) {
      if (!entity.has<UIComponent>())
        continue;

      const UIComponent &cmp = entity.get<UIComponent>();
      if (!cmp.was_rendered_to_screen || cmp.should_hide)
        continue;

      const ValidationViolation &violation = entity.get<ValidationViolation>();
      RectangleType rect = cmp.rect();

      // Apply any modifiers
      if (entity.has<HasUIModifiers>()) {
        rect = entity.get<HasUIModifiers>().apply_modifier(rect);
      }

      // Color based on severity (red for critical, orange for medium, yellow
      // for low)
      Color border_color;
      if (violation.severity >= 0.8f) {
        border_color = Color{255, 0, 0, 200}; // Red for critical
      } else if (violation.severity >= 0.5f) {
        border_color = Color{255, 165, 0, 200}; // Orange for medium
      } else {
        border_color = Color{255, 255, 0, 200}; // Yellow for low
      }

      // Draw a thick border around the violating element
      float thickness = 3.0f;

      // Top edge
      draw_rectangle(RectangleType{rect.x - thickness, rect.y - thickness,
                                   rect.width + thickness * 2, thickness},
                     border_color);
      // Bottom edge
      draw_rectangle(RectangleType{rect.x - thickness, rect.y + rect.height,
                                   rect.width + thickness * 2, thickness},
                     border_color);
      // Left edge
      draw_rectangle(
          RectangleType{rect.x - thickness, rect.y, thickness, rect.height},
          border_color);
      // Right edge
      draw_rectangle(
          RectangleType{rect.x + rect.width, rect.y, thickness, rect.height},
          border_color);

      // Draw a small indicator showing severity
      float indicator_size = 16.0f;
      Color indicator_bg = colors::opacity_pct(border_color, 0.8f);
      draw_rectangle(RectangleType{rect.x - thickness, rect.y - thickness,
                                   indicator_size, indicator_size},
                     indicator_bg);
    }
  }
};

// ============================================================
// System Registration Helpers
// ============================================================

// Register validation update systems
// Call this after register_after_ui_updates()
static void register_update_systems(SystemManager &sm) {
  // Clear previous frame's violations first
  sm.register_update_system(std::make_unique<ClearViolations>());

  // Run validation checks (these only do work if enabled in ValidationConfig)
  sm.register_update_system(std::make_unique<ValidateScreenBounds>());
  sm.register_update_system(std::make_unique<ValidateChildContainment>());
  sm.register_update_system(std::make_unique<ValidateComponentContrast>());
  sm.register_update_system(std::make_unique<ValidateMinFontSize>());
}

// Register validation render overlay
// Call this after registering main render systems
template <typename InputAction>
static void register_render_overlay(SystemManager &sm) {
  sm.register_render_system(std::make_unique<RenderOverlay<InputAction>>());
}

template <typename InputAction>
static void register_systems(SystemManager &sm) {
  // Register update systems (validation checks)
  sm.register_update_system(std::make_unique<ClearViolations>());
  sm.register_update_system(std::make_unique<ValidateScreenBounds>());
  sm.register_update_system(std::make_unique<ValidateChildContainment>());
  sm.register_update_system(std::make_unique<ValidateComponentContrast>());
  sm.register_update_system(std::make_unique<ValidateMinFontSize>());

  // Register render overlay (visual debug)
  sm.register_render_system(std::make_unique<RenderOverlay<InputAction>>());
}

} // namespace validation
} // namespace ui
} // namespace afterhours
