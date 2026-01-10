#pragma once

#include "../color.h"
#include "theme.h"

namespace afterhours {
namespace ui {
namespace imm {

struct ThemeDefaults {
  Theme theme;

  ThemeDefaults() = default;

  // Singleton pattern
  static ThemeDefaults &get() {
    static ThemeDefaults instance;
    return instance;
  }

  // Delete copy constructor and assignment operator
  ThemeDefaults(const ThemeDefaults &) = delete;
  ThemeDefaults &operator=(const ThemeDefaults &) = delete;

  // Theme configuration methods
  ThemeDefaults &set_theme_color(Theme::Usage usage, const Color &color) {
    switch (usage) {
    case Theme::Usage::Primary:
      theme.primary = color;
      break;
    case Theme::Usage::Secondary:
      theme.secondary = color;
      break;
    case Theme::Usage::Accent:
      theme.accent = color;
      break;
    case Theme::Usage::Background:
      theme.background = color;
      break;
    case Theme::Usage::Surface:
      theme.surface = color;
      break;
    case Theme::Usage::Font:
      theme.font = color;
      break;
    case Theme::Usage::DarkFont:
      theme.darkfont = color;
      break;
    case Theme::Usage::FontMuted:
      theme.font_muted = color;
      break;
    case Theme::Usage::Error:
      theme.error = color;
      break;
    default:
      break;
    }
    return *this;
  }

  // Set the entire theme at once
  ThemeDefaults &set_theme(const Theme &new_theme) {
    theme = new_theme;
    return *this;
  }

  Theme get_theme() const { return theme; }

  // Validate that the current theme meets WCAG AA accessibility standards.
  // Returns true if the theme passes, false otherwise.
  // This is an optional utility - not enforced automatically.
  bool validate_theme_accessibility() const {
    bool valid = theme.validate_accessibility();
    if (!valid) {
      log_warn("Theme does not meet WCAG AA contrast requirements. "
               "Consider adjusting font/background colors for better "
               "accessibility.");
    }
    return valid;
  }
};

} // namespace imm
} // namespace ui
} // namespace afterhours