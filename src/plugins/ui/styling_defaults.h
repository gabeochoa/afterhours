#pragma once

#include <cmath>
#include <map>
#include <optional>
#include <string>

#include "../autolayout.h"
#include "theme.h"
#include "validation_config.h"

namespace afterhours {

namespace ui {

namespace imm {

static Vector2Type default_component_size = {200.f, 50.f};

struct DefaultSpacing {
  static Size tiny() { return h720(8.0f); }
  static Size small() { return h720(16.0f); }
  static Size medium() { return h720(24.0f); }
  static Size large() { return h720(32.0f); }
  static Size xlarge() { return h720(48.0f); }
  static Size container() { return h720(64.0f); }
};

struct TypographyScale {
  static constexpr float BASE_SIZE_720P = 16.0f;
  static constexpr float RATIO = 1.25f;
  // Lowered from 18.67 to allow decorative/secondary text at smaller sizes
  // while still ensuring body text remains readable
  static constexpr float MIN_ACCESSIBLE_SIZE_720P = 12.0f;

  static Size size(int level) {
    float px_size = BASE_SIZE_720P * std::pow(RATIO, level);
    return h720(px_size);
  }

  static Size base() { return h720(BASE_SIZE_720P); }
  static Size min_accessible() { return h720(MIN_ACCESSIBLE_SIZE_720P); }

  static float compute_line_height(float font_size_px_720p) {
    return font_size_px_720p * 1.5f;
  }
};

enum class SliderHandleValueLabelPosition {
  None,            // No label
  OnHandle,        // Show the value on the handle
  WithLabel,       // Show the main label with the value
  WithLabelNewLine // Show the main label with the value on a separate line
};

// Component type enum for styling defaults
enum struct ComponentType {
  Button,
  ButtonGroup,
  Div,
  Slider,
  Checkbox,
  CheckboxNoLabel,
  Dropdown,
  Pagination,
  NavigationBar,
  CheckboxGroup,
  Image,
  Separator,
};

// Forward declaration - ComponentConfig is defined in component_config.h
struct ComponentConfig;

// TODO singleton helper
struct UIStylingDefaults {
  std::map<ComponentType, ComponentConfig> component_configs;
  std::string default_font_name = UIComponent::UNSET_FONT;
  float default_font_size = 16.f;
  bool enable_grid_snapping = false;

  // Validation configuration for design rule enforcement
  ValidationConfig validation;

  UIStylingDefaults() = default;

  // Theme configuration methods
  UIStylingDefaults &set_theme_color(Theme::Usage usage, const Color &color) {
    auto &theme_defaults = ThemeDefaults::get();
    theme_defaults.set_theme_color(usage, color);
    return *this;
  }

  UIStylingDefaults &
  set_click_activation_mode(ClickActivationMode activation_mode) {
    auto &theme_defaults = ThemeDefaults::get();
    theme_defaults.set_click_activation_mode(activation_mode);
    return *this;
  }

  // Helper methods for common theme colors
  UIStylingDefaults &set_primary_color(const Color &color) {
    return set_theme_color(Theme::Usage::Primary, color);
  }

  UIStylingDefaults &set_secondary_color(const Color &color) {
    return set_theme_color(Theme::Usage::Secondary, color);
  }

  UIStylingDefaults &set_accent_color(const Color &color) {
    return set_theme_color(Theme::Usage::Accent, color);
  }

  // Font configuration methods
  UIStylingDefaults &set_default_font(const std::string &font_name,
                                      float font_size) {
    default_font_name = font_name;
    default_font_size = font_size;
    return *this;
  }

  // Layout configuration methods
  UIStylingDefaults &set_grid_snapping(bool enabled) {
    enable_grid_snapping = enabled;
    return *this;
  }

  // Validation configuration methods
  UIStylingDefaults &set_validation_mode(ValidationMode mode) {
    validation.mode = mode;
    return *this;
  }

  UIStylingDefaults &enable_development_validation() {
    validation.enable_development_mode();
    return *this;
  }

  UIStylingDefaults &enable_strict_validation() {
    validation.enable_strict_mode();
    return *this;
  }

  UIStylingDefaults &enable_tv_safe_validation() {
    validation.enable_tv_safe_mode();
    return *this;
  }

  // Get current validation config (const reference)
  const ValidationConfig &get_validation_config() const { return validation; }

  // Get mutable validation config for direct modification
  ValidationConfig &get_validation_config_mut() { return validation; }

  // Singleton pattern
  static UIStylingDefaults &get() {
    static UIStylingDefaults instance;
    return instance;
  }

  // Delete copy constructor and assignment operator
  UIStylingDefaults(const UIStylingDefaults &) = delete;
  UIStylingDefaults &operator=(const UIStylingDefaults &) = delete;

  // Set default styling with real ComponentConfig
  UIStylingDefaults &set_component_config(ComponentType component_type,
                                          const ComponentConfig &config);

  // Get real ComponentConfig for a component type
  std::optional<ComponentConfig>
  get_component_config(ComponentType component_type) const;

  // Check if defaults exist for a component type
  bool has_component_defaults(ComponentType component_type) const;

  // Merge component defaults with a config
  ComponentConfig merge_with_defaults(ComponentType component_type,
                                      const ComponentConfig &config) const;
};

template <typename T>
concept HasUIContext = requires(T a) {
  {
    std::is_same_v<T, UIContext<typename decltype(a)::value_type>>
  } -> std::convertible_to<bool>;
};

} // namespace imm

} // namespace ui

} // namespace afterhours
