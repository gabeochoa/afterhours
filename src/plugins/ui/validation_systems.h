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
#include "ui_collection.h"
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
        OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
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
          OptEntity opt_ent = UICollectionHolder::getEntityForID(child.id);
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
  float screen_height = 720.f;

  virtual void once(float) override {
    if (auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>()) {
      screen_height = static_cast<float>(pcr->current_resolution.height);
    }
  }

  virtual void for_each_with(Entity &entity, UIComponent &cmp, HasLabel &label,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_min_font_size)
      return;

    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    float font_size = resolve_to_pixels(cmp.font_size, screen_height);

    if (font_size < config.min_font_size) {
      std::string label_hint;
      if (!label.label.empty()) {
        label_hint = " \"" + label.label.substr(0, 40) + "\"";
      } else if (entity.has<UIComponentDebug>()) {
        label_hint = " [" + entity.get<UIComponentDebug>().name() + "]";
      }

      std::string msg =
          "Font size " + std::to_string(font_size) + "px below minimum " +
          std::to_string(config.min_font_size) + "px" + label_hint;

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

// Validates that UI components use resolution-relative sizing (screen_pct,
// h720, percent) instead of fixed pixels. Components using Dim::Pixels for
// their size, padding, margin, or font size won't scale correctly across
// different screen resolutions.
struct ValidateResolutionIndependence : System<AutoLayoutRoot, UIComponent> {

  static bool is_pixel_dim(const Size &size, float threshold) {
    return size.dim == Dim::Pixels && size.value > threshold;
  }

  static std::string dim_location_name(const std::string &field, Axis axis) {
    switch (axis) {
    case Axis::X:
      return field + ".x";
    case Axis::Y:
      return field + ".y";
    case Axis::left:
      return field + ".left";
    case Axis::top:
      return field + ".top";
    case Axis::right:
      return field + ".right";
    case Axis::bottom:
      return field + ".bottom";
    default:
      return field;
    }
  }

  void validate_resolution(UIComponent &cmp, const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    float threshold = config.resolution_independence_pixel_threshold;
    std::vector<std::string> violations;

    // Check desired size (width/height)
    if (is_pixel_dim(cmp.desired[Axis::X], threshold)) {
      violations.push_back(
          dim_location_name("size", Axis::X) + "=" +
          std::to_string(static_cast<int>(cmp.desired[Axis::X].value)) + "px");
    }
    if (is_pixel_dim(cmp.desired[Axis::Y], threshold)) {
      violations.push_back(
          dim_location_name("size", Axis::Y) + "=" +
          std::to_string(static_cast<int>(cmp.desired[Axis::Y].value)) + "px");
    }

    // Check padding
    for (auto axis : {Axis::top, Axis::left, Axis::bottom, Axis::right}) {
      if (is_pixel_dim(cmp.desired_padding[axis], threshold)) {
        violations.push_back(
            dim_location_name("padding", axis) + "=" +
            std::to_string(static_cast<int>(cmp.desired_padding[axis].value)) +
            "px");
      }
    }

    // Check margin
    for (auto axis : {Axis::top, Axis::left, Axis::bottom, Axis::right}) {
      if (is_pixel_dim(cmp.desired_margin[axis], threshold)) {
        violations.push_back(
            dim_location_name("margin", axis) + "=" +
            std::to_string(static_cast<int>(cmp.desired_margin[axis].value)) +
            "px");
      }
    }

    // Check font size
    if (is_pixel_dim(cmp.font_size, threshold)) {
      violations.push_back(
          "font_size=" + std::to_string(static_cast<int>(cmp.font_size.value)) +
          "px");
    }

    if (!violations.empty()) {
      // Build a single message listing all pixel-based fields
      std::string fields;
      for (size_t i = 0; i < violations.size(); ++i) {
        if (i > 0)
          fields += ", ";
        fields += violations[i];
      }

      // Get component debug name if available
      std::string name_hint;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
      if (opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (ent.has<UIComponentDebug>()) {
          name_hint = " [" + ent.get<UIComponentDebug>().name() + "]";
        }
      }

      std::string msg =
          "Uses fixed pixels instead of resolution-relative units" + name_hint +
          ": " + fields + ". Use h720(), screen_pct(), or percent() instead.";

      report_violation(config, "ResolutionIndependence", msg, cmp.id, 0.7f);

      // Add violation component for visual debugging
      if (config.highlight_violations) {
        if (opt_ent.valid()) {
          Entity &ent = opt_ent.asE();
          if (!ent.has<ValidationViolation>()) {
            ent.addComponent<ValidationViolation>(msg, "ResolutionIndependence",
                                                  0.7f);
          }
        }
      }
    }

    // Recursively check children
    for (EntityID child_id : cmp.children) {
      validate_resolution(AutoLayout::to_cmp_static(child_id), config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();

    if (!config.enforce_resolution_independence)
      return;

    validate_resolution(cmp, config);
  }
};

// Validates that elements don't resolve to zero width or height
// (common with percent(1.0) when parent has no size, or children() with none)
struct ValidateZeroSize : System<AutoLayoutRoot, UIComponent> {

  void validate_zero_size(UIComponent &cmp, const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    auto rect = cmp.rect();
    bool zero_w = rect.width < 0.5f;
    bool zero_h = rect.height < 0.5f;

    if (zero_w || zero_h) {
      std::string dims;
      if (zero_w)
        dims += "width=0 ";
      if (zero_h)
        dims += "height=0 ";

      // Get debug name if available
      std::string name_hint;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
      if (opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (ent.has<UIComponentDebug>()) {
          name_hint = " [" + ent.get<UIComponentDebug>().name() + "]";
        }
      }

      std::string msg = "Element resolved to zero size" + name_hint + ": " +
                        dims +
                        "Check parent has explicit size if using percent(), "
                        "or element has children if using children().";

      report_violation(config, "ZeroSize", msg, cmp.id, 0.8f);

      if (config.highlight_violations && opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (!ent.has<ValidationViolation>()) {
          ent.addComponent<ValidationViolation>(msg, "ZeroSize", 0.8f);
        }
      }
    }

    for (EntityID child_id : cmp.children) {
      validate_zero_size(AutoLayout::to_cmp_static(child_id), config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();
    if (!config.enforce_zero_size_detection)
      return;
    validate_zero_size(cmp, config);
  }
};

// Validates that absolute-positioned elements don't have non-zero margins
// (margins on absolute elements act as position offsets, not spacing)
struct ValidateAbsoluteMarginConflict : System<AutoLayoutRoot, UIComponent> {

  static bool has_nonzero_margin(const UIComponent &cmp) {
    for (auto axis : {Axis::top, Axis::left, Axis::bottom, Axis::right}) {
      if (cmp.desired_margin[axis].value > 0.001f &&
          cmp.desired_margin[axis].dim != Dim::None) {
        return true;
      }
    }
    return false;
  }

  void validate_margin_conflict(UIComponent &cmp,
                                const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    if (cmp.absolute && has_nonzero_margin(cmp)) {
      std::string name_hint;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
      if (opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (ent.has<UIComponentDebug>()) {
          name_hint = " [" + ent.get<UIComponentDebug>().name() + "]";
        }
      }

      std::string msg = "Absolute element has margins" + name_hint +
                        ". On absolute elements, margins are position offsets "
                        "(they don't create spacing). Use with_translate() for "
                        "positioning instead.";

      report_violation(config, "AbsoluteMarginConflict", msg, cmp.id, 0.6f);

      if (config.highlight_violations && opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (!ent.has<ValidationViolation>()) {
          ent.addComponent<ValidationViolation>(msg, "AbsoluteMarginConflict",
                                                0.6f);
        }
      }
    }

    for (EntityID child_id : cmp.children) {
      validate_margin_conflict(AutoLayout::to_cmp_static(child_id), config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();
    if (!config.enforce_absolute_margin_conflict)
      return;
    validate_margin_conflict(cmp, config);
  }
};

// Validates that elements with labels have a font set
// (font_name != UNSET_FONT, meaning either explicit or default was applied)
struct ValidateLabelHasFont : System<UIComponent, HasLabel> {
  virtual void for_each_with(Entity &entity, UIComponent &cmp, HasLabel &label,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();
    if (!config.enforce_label_has_font)
      return;
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    if (cmp.font_name == UIComponent::UNSET_FONT) {
      std::string msg =
          "Element has label \"" + label.label +
          "\" but no font set. Text may not render. "
          "Use .with_font(), set_default_font(), or ensure font inheritance.";

      report_violation(config, "LabelNoFont", msg, entity.id, 0.9f);

      if (config.highlight_violations) {
        if (!entity.has<ValidationViolation>()) {
          entity.addComponent<ValidationViolation>(msg, "LabelNoFont", 0.9f);
        }
      }
    }
  }
};

// Validates that computed margins and padding follow 4/8/16 spacing rhythm
struct ValidateSpacingRhythm : System<AutoLayoutRoot, UIComponent> {

  void validate_spacing(UIComponent &cmp, const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    std::vector<std::string> violations;

    // Check computed margins
    for (auto axis : {Axis::top, Axis::left, Axis::bottom, Axis::right}) {
      float val = cmp.computed_margin[axis];
      if (val > 0.001f && !is_valid_spacing(val)) {
        violations.push_back("margin." +
                             std::string(axis == Axis::top      ? "top"
                                         : axis == Axis::left   ? "left"
                                         : axis == Axis::bottom ? "bottom"
                                                                : "right") +
                             "=" + std::to_string(static_cast<int>(val)) +
                             "px");
      }
    }

    // Check computed padding
    for (auto axis : {Axis::top, Axis::left, Axis::bottom, Axis::right}) {
      float val = cmp.computed_padd[axis];
      if (val > 0.001f && !is_valid_spacing(val)) {
        violations.push_back("padding." +
                             std::string(axis == Axis::top      ? "top"
                                         : axis == Axis::left   ? "left"
                                         : axis == Axis::bottom ? "bottom"
                                                                : "right") +
                             "=" + std::to_string(static_cast<int>(val)) +
                             "px");
      }
    }

    if (!violations.empty()) {
      std::string fields;
      for (size_t i = 0; i < violations.size(); ++i) {
        if (i > 0)
          fields += ", ";
        fields += violations[i];
      }

      std::string name_hint;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
      if (opt_ent.valid() && opt_ent.asE().has<UIComponentDebug>()) {
        name_hint = " [" + opt_ent.asE().get<UIComponentDebug>().name() + "]";
      }

      std::string msg = "Spacing not on 4px rhythm" + name_hint + ": " +
                        fields + ". Use multiples of 4 (4, 8, 12, 16, 24, 32).";

      report_violation(config, "SpacingRhythm", msg, cmp.id, 0.4f);

      if (config.highlight_violations && opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (!ent.has<ValidationViolation>()) {
          ent.addComponent<ValidationViolation>(msg, "SpacingRhythm", 0.4f);
        }
      }
    }

    for (EntityID child_id : cmp.children) {
      validate_spacing(AutoLayout::to_cmp_static(child_id), config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();
    if (!config.enforce_spacing_rhythm)
      return;
    validate_spacing(cmp, config);
  }
};

// Validates that computed positions are pixel-aligned (no fractional pixels)
// to prevent blurry rendering on non-retina displays
struct ValidatePixelAlignment : System<AutoLayoutRoot, UIComponent> {

  void validate_alignment(UIComponent &cmp, const ValidationConfig &config) {
    if (!cmp.was_rendered_to_screen || cmp.should_hide)
      return;

    auto rect = cmp.rect();
    bool x_misaligned = !is_pixel_aligned(rect.x);
    bool y_misaligned = !is_pixel_aligned(rect.y);

    if (x_misaligned || y_misaligned) {
      std::string name_hint;
      OptEntity opt_ent = UICollectionHolder::getEntityForID(cmp.id);
      if (opt_ent.valid() && opt_ent.asE().has<UIComponentDebug>()) {
        name_hint = " [" + opt_ent.asE().get<UIComponentDebug>().name() + "]";
      }

      std::string msg = "Element not pixel-aligned" + name_hint + " at (" +
                        std::to_string(rect.x) + ", " + std::to_string(rect.y) +
                        "). Fractional positions cause blurry rendering.";

      report_violation(config, "PixelAlignment", msg, cmp.id, 0.3f);

      if (config.highlight_violations && opt_ent.valid()) {
        Entity &ent = opt_ent.asE();
        if (!ent.has<ValidationViolation>()) {
          ent.addComponent<ValidationViolation>(msg, "PixelAlignment", 0.3f);
        }
      }
    }

    for (EntityID child_id : cmp.children) {
      validate_alignment(AutoLayout::to_cmp_static(child_id), config);
    }
  }

  virtual void for_each_with(Entity &, AutoLayoutRoot &, UIComponent &cmp,
                             float) override {
    auto &styling_defaults = imm::UIStylingDefaults::get();
    const auto &config = styling_defaults.get_validation_config();
    if (!config.enforce_pixel_alignment)
      return;
    validate_alignment(cmp, config);
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
  sm.register_update_system(std::make_unique<ValidateResolutionIndependence>());
  sm.register_update_system(std::make_unique<ValidateZeroSize>());
  sm.register_update_system(std::make_unique<ValidateAbsoluteMarginConflict>());
  sm.register_update_system(std::make_unique<ValidateLabelHasFont>());
  sm.register_update_system(std::make_unique<ValidateSpacingRhythm>());
  sm.register_update_system(std::make_unique<ValidatePixelAlignment>());
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
  sm.register_update_system(std::make_unique<ValidateResolutionIndependence>());
  sm.register_update_system(std::make_unique<ValidateZeroSize>());
  sm.register_update_system(std::make_unique<ValidateAbsoluteMarginConflict>());
  sm.register_update_system(std::make_unique<ValidateLabelHasFont>());
  sm.register_update_system(std::make_unique<ValidateSpacingRhythm>());
  sm.register_update_system(std::make_unique<ValidatePixelAlignment>());

  // Register render overlay (visual debug)
  sm.register_render_system(std::make_unique<RenderOverlay<InputAction>>());
}

} // namespace validation
} // namespace ui
} // namespace afterhours
