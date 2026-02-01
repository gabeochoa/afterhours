#pragma once

#include <bitset>
#include <cassert>
#include <map>
#include <string>

#include "../color.h"
#include "../translation.h"

namespace afterhours {

namespace ui {

enum class ClickActivationMode {
  Default,
  Press,
  Release,
};

// Text stroke/outline configuration for labels
// Renders text with an outline effect by drawing the text at offsets
struct TextStroke {
  Color color = Color{0, 0, 0, 255};
  float thickness = 2.0f;

  bool has_stroke() const { return thickness > 0.0f && color.a > 0; }

  // Factory method for common use case
  static TextStroke with_color(Color c, float t = 2.0f) {
    return TextStroke{c, t};
  }
};

// Text drop shadow configuration for labels
// Renders a shadow behind text by drawing the text at an offset
struct TextShadow {
  Color color = Color{0, 0, 0, 128};
  float offset_x = 2.0f;
  float offset_y = 2.0f;

  bool has_shadow() const { return color.a > 0; }

  // Factory methods for common use cases
  static TextShadow with_color(Color c, float ox = 2.0f, float oy = 2.0f) {
    return TextShadow{c, ox, oy};
  }

  static TextShadow soft(float ox = 2.0f, float oy = 2.0f) {
    return TextShadow{Color{0, 0, 0, 80}, ox, oy};
  }

  static TextShadow hard(float ox = 2.0f, float oy = 2.0f) {
    return TextShadow{Color{0, 0, 0, 180}, ox, oy};
  }
};

// Font configuration for a specific language
struct FontConfig {
  // TODO: Investigate using FontID enum instead of strings for type safety
  std::string font_name;   // Key in FontManager
  float size_scale = 1.0f; // Multiplier for this font's visual size

  FontConfig() = default;
  FontConfig(const std::string &name, float scale = 1.0f)
      : font_name(name), size_scale(scale) {}
};

struct Theme {
  enum struct Usage {
    Font,
    DarkFont,
    FontMuted, // New: for secondary/muted text
    Background,
    Surface, // New: for cards/panels (slightly different from background)
    Primary,
    Secondary,
    Accent,
    Error,
    Focus, // Dedicated focus ring color for accessibility

    //
    Custom,
    Default,
    None,
  };

  static bool is_valid(Usage cu) {
    switch (cu) {
    case Usage::Font:
    case Usage::DarkFont:
    case Usage::FontMuted:
    case Usage::Background:
    case Usage::Surface:
    case Usage::Primary:
    case Usage::Secondary:
    case Usage::Accent:
    case Usage::Error:
    case Usage::Focus:
      return true;
    case Usage::Custom:
    case Usage::Default:
    case Usage::None:
      return false;
    };
    return false;
  }

  // Default to pure white/black for auto_text_color to work properly
  Color font{255, 255, 255, 255};       // White - for dark backgrounds
  Color darkfont{30, 30, 30, 255};      // Near-black - for light backgrounds
  Color font_muted{150, 150, 150, 255}; // Gray - for secondary text
  Color background{45, 45, 55, 255};    // Dark gray
  Color surface{60, 60, 70, 255};       // Slightly lighter gray

  Color primary{100, 140, 200, 255};  // Blue
  Color secondary{80, 100, 140, 255}; // Dark blue
  Color accent{200, 160, 100, 255};   // Gold
  Color error{200, 80, 80, 255};      // Red
  Color focus{255, 255, 255, 255};    // White - high contrast focus ring

  // Focus ring configuration
  float focus_ring_thickness = 3.0f;  // Thickness of focus ring outline (2-3px for visibility)
  float focus_ring_offset = 4.0f;     // Gap between element and focus ring (ensures no clipping)

  ClickActivationMode click_activation_mode = ClickActivationMode::Press;

  // ===== Font configuration =====
  // Per-language font configuration
  std::map<translation::Language, FontConfig> language_fonts;

  // Base font sizes (in pixels)
  float font_size_sm = 16.f;
  float font_size_md = 20.f;
  float font_size_lg = 32.f;
  float font_size_xl = 42.f;

  // Get font config for a language
  const FontConfig &get_font_config(translation::Language lang) const {
    auto it = language_fonts.find(lang);
    if (it == language_fonts.end()) {
      log_warn("Theme missing font config for language: {}",
               magic_enum::enum_name(lang));
      // Return first available as fallback
      assert(!language_fonts.empty() &&
             "Theme must define at least one language font");
      return language_fonts.begin()->second;
    }
    return it->second;
  }

  // Get a font size scaled for the language's font
  float get_scaled_font_size(translation::Language lang,
                             float base_size) const {
    return base_size * get_font_config(lang).size_scale;
  }

  // Convenience: get font name for current language
  std::string get_font_name(translation::Language lang) const {
    return get_font_config(lang).font_name;
  }

  Theme()
      : font(colors::isabelline), darkfont(colors::oxford_blue),
        font_muted(colors::darken(colors::isabelline, 0.3f)),
        background(colors::oxford_blue),
        surface(colors::lighten(colors::oxford_blue, 0.1f)),
        primary(colors::pacific_blue), secondary(colors::tea_green),
        accent(colors::orange_soda),
        // TODO find a better error color
        error(colors::red),
        focus(colors::isabelline) {} // White focus ring for high contrast

  // Legacy 7-arg constructor for backwards compatibility
  Theme(Color f, Color df, Color bg, Color p, Color s, Color a, Color e)
      : font(f), darkfont(df), font_muted(colors::darken(f, 0.3f)),
        background(bg), surface(colors::lighten(bg, 0.1f)), primary(p),
        secondary(s), accent(a), error(e), focus(f) {} // Default focus to font color

  // Full constructor with all colors
  Theme(Color f, Color df, Color fm, Color bg, Color surf, Color p, Color s,
        Color a, Color e)
      : font(f), darkfont(df), font_muted(fm), background(bg), surface(surf),
        primary(p), secondary(s), accent(a), error(e), focus(f) {} // Default focus to font color

  Color from_usage(Usage cu, bool disabled = false) const {
    Color color;
    switch (cu) {
    case Usage::Font:
      color = font;
      break;
    case Usage::DarkFont:
      color = darkfont;
      break;
    case Usage::FontMuted:
      color = font_muted;
      break;
    case Usage::Background:
      color = background;
      break;
    case Usage::Surface:
      color = surface;
      break;
    case Usage::Primary:
      color = primary;
      break;
    case Usage::Secondary:
      color = secondary;
      break;
    case Usage::Accent:
      color = accent;
      break;
    case Usage::Error:
      color = error;
      break;
    case Usage::Focus:
      color = focus;
      break;
    case Usage::Default:
      log_warn("You should not be fetching 'default' color usage from "
               "theme, "
               "UI library should handle this??");
      color = primary;
      break;
    case Usage::Custom:
      log_warn("You should not be fetching 'custom' color usage from "
               "theme, "
               "UI library should handle this??");
      color = primary;
      break;
    case Usage::None:
      log_warn("You should not be fetching 'none' color usage from theme, "
               "UI library should handle this??");
      color = primary;
      break;
    }
    if (disabled) {
      return colors::darken(color, 0.3f);
    }
    return color;
  }

  // Automatically pick the best font color for a given background usage
  // Uses the theme's font/darkfont and picks whichever has better contrast
  Color auto_font_for(Usage background_usage) const {
    Color bg = from_usage(background_usage);
    return colors::auto_text_color(bg, font, darkfont);
  }

  // Validate that the theme meets WCAG AA accessibility standards
  // Checks font on background and darkfont on surface
  bool validate_accessibility() const {
    return colors::meets_wcag_aa(font, background) &&
           colors::meets_wcag_aa(darkfont, surface);
  }

  std::bitset<4> rounded_corners = std::bitset<4>().set();
  float roundness = 0.5f; // 0.0 = sharp corners, 1.0 = fully rounded
  int segments = 8;       // Number of segments per rounded corner
};

namespace imm {

// Singleton for managing global theme defaults
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
    case Theme::Usage::Focus:
      theme.focus = color;
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

  ThemeDefaults &set_click_activation_mode(ClickActivationMode mode) {
    theme.click_activation_mode = mode;
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
