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
    case Theme::Usage::Font:
      theme.font = color;
      break;
    case Theme::Usage::DarkFont:
      theme.darkfont = color;
      break;
    case Theme::Usage::Error:
      theme.error = color;
      break;
    default:
      break;
    }
    return *this;
  }

  Theme get_theme() const { return theme; }
};

} // namespace imm
} // namespace ui
} // namespace afterhours