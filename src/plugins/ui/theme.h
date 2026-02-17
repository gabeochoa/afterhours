#pragma once

#include <bitset>
#include <cassert>
#include <cmath>
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

// Font sizing with auto-interpolation for missing values
// Positive values = user-set, negative values = interpolated
// Call finalize() after setting values to compute interpolated ones
struct FontSizing {
  float small = -14.0f;  // Default interpolated
  float medium = -20.0f; // Default interpolated
  float large = -28.0f;  // Default interpolated
  float xl = -38.0f;     // Default interpolated

  enum class Tier { Small, Medium, Large, XL };

  // Returns the font size for a tier (always positive)
  float get(Tier tier) const {
    switch (tier) {
    case Tier::Small:
      return std::abs(small);
    case Tier::Medium:
      return std::abs(medium);
    case Tier::Large:
      return std::abs(large);
    case Tier::XL:
      return std::abs(xl);
    }
    return std::abs(medium);
  }

  // Check if a value was user-set (positive) vs interpolated (negative)
  bool is_user_set(Tier tier) const {
    switch (tier) {
    case Tier::Small:
      return small > 0;
    case Tier::Medium:
      return medium > 0;
    case Tier::Large:
      return large > 0;
    case Tier::XL:
      return xl > 0;
    }
    return false;
  }

  // clang-format off
  // Compute interpolated values from user-set ones
  FontSizing &finalize() {
    // Find first and last user-set tier indices
    int first = (small > 0) ? 0 : (medium > 0) ? 1 : (large > 0) ? 2 : (xl > 0) ? 3 : -1;
    int last = (xl > 0) ? 3 : (large > 0) ? 2 : (medium > 0) ? 1 : (small > 0) ? 0 : -1;

    if (first < 0) return *this;  // No user values, keep defaults

    float first_val = (first == 0) ? small : (first == 1) ? medium : (first == 2) ? large : xl;

    if (first == last) {  // One value - use for all unset
      if (small < 0) small = -first_val;
      if (medium < 0) medium = -first_val;
      if (large < 0) large = -first_val;
      if (xl < 0) xl = -first_val;
      return *this;
    }

    float last_val = (last == 0) ? small : (last == 1) ? medium : (last == 2) ? large : xl;
    float step = (last_val - first_val) / float(last - first);

    if (small < 0) small = -(first_val + step * (0 - first));
    if (medium < 0) medium = -(first_val + step * (1 - first));
    if (large < 0) large = -(first_val + step * (2 - first));
    if (xl < 0) xl = -(first_val + step * (3 - first));
    return *this;
  }
  // clang-format on
};

using FontSize = FontSizing::Tier;

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

  // Get a reference to a color by usage
  // Returns primary for Custom/Default/None (invalid usages)
  Color &color_ref(Usage cu) {
    switch (cu) {
    case Usage::Font:
      return font;
    case Usage::DarkFont:
      return darkfont;
    case Usage::FontMuted:
      return font_muted;
    case Usage::Background:
      return background;
    case Usage::Surface:
      return surface;
    case Usage::Primary:
      return primary;
    case Usage::Secondary:
      return secondary;
    case Usage::Accent:
      return accent;
    case Usage::Error:
      return error;
    case Usage::Focus:
      return focus;
    default:
      return primary;
    }
  }

  const Color &color_ref(Usage cu) const {
    return const_cast<Theme *>(this)->color_ref(cu);
  }

  // Set a color by usage
  void set_color(Usage cu, const Color &c) { color_ref(cu) = c; }

  // Focus ring configuration
  float focus_ring_thickness =
      3.0f; // Thickness of focus ring outline (2-3px for visibility)
  float focus_ring_offset =
      4.0f; // Gap between element and focus ring (ensures no clipping)

  ClickActivationMode click_activation_mode = ClickActivationMode::Press;

  // Disabled element styling.
  // When with_disabled(true) is set on a ComponentConfig:
  //  1. The background color is desaturated using disabled_opacity
  //  2. The color is shifted toward grayscale (50% desaturation)
  //  3. The element does NOT respond to hover or click
  //  4. Focus can still move to disabled elements (for accessibility)
  //     but they won't activate
  float disabled_opacity = 0.3f;

  // ===== UI Scale =====
  // Controls the size of pixel-based UI elements in Adaptive scaling mode.
  // 1.0 = 100% (default), 1.5 = 150%, etc.
  // In Proportional mode this is ignored. In Adaptive mode, all pixels()
  // values are multiplied by this factor (like browser Ctrl+/- zoom).
  float ui_scale = 1.0f;

  // ===== Font configuration =====
  // Per-language font configuration
  std::map<translation::Language, FontConfig> language_fonts;

  FontSizing font_sizing;

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

  // Default constructor - uses a dark theme with good defaults
  Theme()
      : font(colors::isabelline), darkfont(colors::oxford_blue),
        font_muted(colors::darken(colors::isabelline, 0.25f)),
        background(colors::oxford_blue),
        surface(colors::lighten(colors::oxford_blue, 0.1f)),
        primary(colors::pacific_blue), secondary(colors::tea_green),
        accent(colors::orange_soda), error(colors::red),
        focus(colors::isabelline) {}

  Color from_usage(Usage cu, bool disabled = false) const {
    if (!is_valid(cu)) {
      log_warn("You should not be fetching '{}' color usage from theme, "
               "UI library should handle this??",
               magic_enum::enum_name(cu));
    }
    Color color = color_ref(cu);
    if (disabled) {
      // Blend toward background and reduce alpha for a clear "disabled" look.
      Color muted = colors::mix(color, background, 1.0f - disabled_opacity);
      muted.a = static_cast<unsigned char>(color.a * disabled_opacity);
      // Desaturate: shift RGB toward grayscale (50% desaturation) so disabled
      // elements look clearly "grayed out" rather than just slightly faded.
      float lum = 0.299f * muted.r + 0.587f * muted.g + 0.114f * muted.b;
      muted.r = static_cast<unsigned char>(muted.r * 0.5f + lum * 0.5f);
      muted.g = static_cast<unsigned char>(muted.g * 0.5f + lum * 0.5f);
      muted.b = static_cast<unsigned char>(muted.b * 0.5f + lum * 0.5f);
      return muted;
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

  // Deprecated: use font_sizing.get(Tier) instead
  // These are kept for backwards compatibility
  float font_size_sm() const {
    return font_sizing.get(FontSizing::Tier::Small);
  }
  float font_size_md() const {
    return font_sizing.get(FontSizing::Tier::Medium);
  }
  float font_size_lg() const {
    return font_sizing.get(FontSizing::Tier::Large);
  }
  float font_size_xl() const { return font_sizing.get(FontSizing::Tier::XL); }

  // Builder pattern - use Theme::create() to start building
  class Builder;
  static Builder create();
};

// Theme builder for fluent API:
// auto theme = Theme::create()
//     .with_palette({.background = {25, 45, 75}, .primary = {85, 145, 215}})
//     .with_font("MyFont")
//     .with_font_sizing({.small = 14, .large = 32})
//     .with_roundness(0.08f);
class Theme::Builder {
  Theme theme_;

public:
  // Color palette - all fields optional, missing ones auto-generated
  // Use alpha=0 to indicate "not set" (e.g., Color{25, 45, 75} is set,
  // Color{0,0,0,0} is not)
  struct Palette {
    Color background{0, 0, 0, 0};
    Color surface{0, 0, 0, 0};
    Color primary{0, 0, 0, 0};
    Color secondary{0, 0, 0, 0};
    Color accent{0, 0, 0, 0};
    Color error{0, 0, 0, 0};
    Color font{0, 0, 0, 0};
    Color darkfont{0, 0, 0, 0};
    Color font_muted{0, 0, 0, 0};
    Color focus{0, 0, 0, 0};

    bool has(const Color &c) const { return c.a > 0; }
  };

  Builder() = default;

  // Apply a color palette, auto-generating missing colors
  Builder &with_palette(const Palette &p) {
    // Apply explicitly set colors
    if (p.has(p.background))
      theme_.background = p.background;
    if (p.has(p.surface))
      theme_.surface = p.surface;
    if (p.has(p.primary))
      theme_.primary = p.primary;
    if (p.has(p.secondary))
      theme_.secondary = p.secondary;
    if (p.has(p.accent))
      theme_.accent = p.accent;
    if (p.has(p.error))
      theme_.error = p.error;
    if (p.has(p.font))
      theme_.font = p.font;
    if (p.has(p.darkfont))
      theme_.darkfont = p.darkfont;
    if (p.has(p.font_muted))
      theme_.font_muted = p.font_muted;
    if (p.has(p.focus))
      theme_.focus = p.focus;

    // Auto-generate missing colors from what we have
    // Surface from background (lighten 15%)
    if (!p.has(p.surface) && p.has(p.background)) {
      theme_.surface = colors::lighten(theme_.background, 0.15f);
    }

    // Secondary from surface (lighten 20%)
    if (!p.has(p.secondary)) {
      theme_.secondary = colors::lighten(theme_.surface, 0.20f);
    }

    // Error defaults to red
    if (!p.has(p.error)) {
      theme_.error = Color{180, 80, 80, 255};
    }

    // Font colors: auto-pick based on background luminance
    if (!p.has(p.font) && !p.has(p.darkfont)) {
      float bg_lum = colors::luminance(theme_.background);
      if (bg_lum < 0.5f) {
        // Dark background - light font, dark darkfont
        theme_.font = Color{235, 240, 245, 255};
        theme_.darkfont = theme_.background;
      } else {
        // Light background - dark font, light darkfont
        theme_.font = Color{30, 30, 30, 255};
        theme_.darkfont = Color{235, 240, 245, 255};
      }
    }

    // Font muted from font at 70% brightness to maintain WCAG AA 4.5:1 contrast
    if (!p.has(p.font_muted)) {
      theme_.font_muted = colors::darken(theme_.font, 0.3f);
    }

    // Focus defaults to font color
    if (!p.has(p.focus)) {
      theme_.focus = theme_.font;
    }

    return *this;
  }

  // Set font family name
  Builder &with_font(const std::string &font_name) {
    theme_.language_fonts[translation::Language::English] =
        FontConfig(font_name, 1.0f);
    return *this;
  }

  // Set font sizing
  Builder &with_font_sizing(FontSizing sizing) {
    sizing.finalize();
    theme_.font_sizing = sizing;
    return *this;
  }

  // Set corner roundness
  Builder &with_roundness(float r) {
    theme_.roundness = r;
    return *this;
  }

  // Set corner segments
  Builder &with_segments(int s) {
    theme_.segments = s;
    return *this;
  }

  // Set any color by usage
  Builder &with_color(Theme::Usage usage, const Color &c) {
    theme_.set_color(usage, c);
    return *this;
  }

  // Set disabled element opacity (0.0 = invisible, 1.0 = fully opaque)
  Builder &with_disabled_opacity(float opacity) {
    theme_.disabled_opacity = std::clamp(opacity, 0.0f, 1.0f);
    return *this;
  }

  // Set UI scale (zoom level). 1.0 = 100%, 1.5 = 150%.
  // Only affects Adaptive scaling mode. Clamped to [0.5, 3.0].
  Builder &with_ui_scale(float scale) {
    theme_.ui_scale = std::clamp(scale, 0.5f, 3.0f);
    return *this;
  }

  // Implicit conversion to Theme
  operator Theme() const { return theme_; }

  // Explicit build method
  Theme build() const { return theme_; }
};

inline Theme::Builder Theme::create() { return Builder(); }

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
    theme.set_color(usage, color);
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

// ============================================================================
// Backwards compatibility aliases - remove after migration
// ============================================================================

// ThemePalette was moved inside Theme::Builder as Theme::Builder::Palette
using ThemePalette = Theme::Builder::Palette;

// Legacy Theme constructors - use Theme::create() builder instead
// 7-arg: Theme(font, darkfont, background, primary, secondary, accent, error)
inline Theme make_theme(Color f, Color df, Color bg, Color p, Color s, Color a,
                        Color e) {
  return Theme::create()
      .with_palette({.background = bg,
                     .primary = p,
                     .secondary = s,
                     .accent = a,
                     .error = e,
                     .font = f,
                     .darkfont = df})
      .build();
}

// 9-arg: Theme(font, darkfont, font_muted, background, surface, primary,
// secondary, accent, error)
inline Theme make_theme(Color f, Color df, Color fm, Color bg, Color surf,
                        Color p, Color s, Color a, Color e) {
  return Theme::create()
      .with_palette({.background = bg,
                     .surface = surf,
                     .primary = p,
                     .secondary = s,
                     .accent = a,
                     .error = e,
                     .font = f,
                     .darkfont = df,
                     .font_muted = fm})
      .build();
}

} // namespace ui

} // namespace afterhours
