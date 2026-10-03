
#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "../core/base_component.h"
#include "../developer.h"

namespace afterhours {

#ifdef AFTER_HOURS_USE_RAYLIB
using Color = raylib::Color;
namespace colors {
constexpr Color UI_BLACK = raylib::BLACK;
constexpr Color UI_RED = raylib::RED;
constexpr Color UI_GREEN = raylib::GREEN;
constexpr Color UI_BLUE = raylib::BLUE;
constexpr Color UI_WHITE = raylib::RAYWHITE;
constexpr Color UI_PINK = raylib::PINK;
} // namespace colors

#else
using Color = ColorType;
namespace colors {
constexpr Color UI_BLACK = {0, 0, 0, 255};
constexpr Color UI_RED = {255, 0, 0, 255};
constexpr Color UI_GREEN = {0, 255, 0, 255};
constexpr Color UI_BLUE = {0, 0, 255, 255};
constexpr Color UI_WHITE = {255, 255, 255, 255};
constexpr Color UI_PINK = {250, 200, 200, 255};
} // namespace colors

#endif

namespace colors {

// ============================================================
// Constexpr Color Constructors
// ============================================================

// Named basics
constexpr Color white() { return {255, 255, 255, 255}; }
constexpr Color black() { return {0, 0, 0, 255}; }
constexpr Color transparent() { return {0, 0, 0, 0}; }

// Grayscale
constexpr Color gray(unsigned char value) { return {value, value, value, 255}; }
constexpr Color gray_25() { return gray(64); }
constexpr Color gray_50() { return gray(128); }
constexpr Color gray_75() { return gray(192); }

// Explicit RGB/RGBA constructors
constexpr Color rgb(unsigned char r, unsigned char g, unsigned char b) {
  return {r, g, b, 255};
}
constexpr Color rgba(unsigned char r, unsigned char g, unsigned char b,
                     unsigned char a) {
  return {r, g, b, a};
}

// Hex color constructor (0xRRGGBB or 0xRRGGBBAA)
constexpr Color hex(uint32_t value) {
  if (value <= 0xFFFFFF) {
    return {static_cast<unsigned char>((value >> 16) & 0xFF),
            static_cast<unsigned char>((value >> 8) & 0xFF),
            static_cast<unsigned char>(value & 0xFF), 255};
  }
  return {static_cast<unsigned char>((value >> 24) & 0xFF),
          static_cast<unsigned char>((value >> 16) & 0xFF),
          static_cast<unsigned char>((value >> 8) & 0xFF),
          static_cast<unsigned char>(value & 0xFF)};
}

// Semantic UI colors
constexpr Color error() { return hex(0xDC3545); }
constexpr Color warning() { return hex(0xFFC107); }
constexpr Color success() { return hex(0x28A745); }
constexpr Color info() { return hex(0x17A2B8); }

// ============================================================
// Legacy named color constants (prefer functions above)
// ============================================================

static const Color red = UI_RED;
static const Color transleucent_green = Color{0, 250, 50, 5};
static const Color transleucent_red = Color{250, 0, 50, 5};
static const Color pacific_blue = Color{71, 168, 189, 255};
static const Color oxford_blue = Color{12, 27, 51, 255};
static const Color orange_soda = Color{240, 100, 73, 255};
static const Color isabelline = Color{237, 230, 227, 255};
static const Color tea_green =
    Color{166, 185, 189, 255}; // 195, 232, 189, 255};

// Soft UI colors for settings/icons
static const Color soft_green = Color{85, 175, 125, 255};
static const Color soft_blue = Color{65, 145, 215, 255};
static const Color soft_purple = Color{145, 95, 185, 255};
static const Color soft_red = Color{215, 85, 85, 255};
static const Color soft_orange = Color{235, 155, 65, 255};
static const Color soft_yellow = Color{215, 155, 65, 255};

static Color darken(const Color &color, const float factor = 0.8f) {
  Color darkerColor;
  darkerColor.r = static_cast<unsigned char>(color.r * factor);
  darkerColor.g = static_cast<unsigned char>(color.g * factor);
  darkerColor.b = static_cast<unsigned char>(color.b * factor);
  darkerColor.a = color.a;
  return darkerColor;
}

static Color lighten(const Color &color, const float amount = 0.2f) {
  Color lighterColor;
  lighterColor.r = static_cast<unsigned char>(
      std::min(255, static_cast<int>(color.r + (255 - color.r) * amount)));
  lighterColor.g = static_cast<unsigned char>(
      std::min(255, static_cast<int>(color.g + (255 - color.g) * amount)));
  lighterColor.b = static_cast<unsigned char>(
      std::min(255, static_cast<int>(color.b + (255 - color.b) * amount)));
  lighterColor.a = color.a;
  return lighterColor;
}

static Color lerp(const Color &a, const Color &b, float t) {
  return Color{
      static_cast<unsigned char>(a.r + static_cast<int>((b.r - a.r) * t)),
      static_cast<unsigned char>(a.g + static_cast<int>((b.g - a.g) * t)),
      static_cast<unsigned char>(a.b + static_cast<int>((b.b - a.b) * t)),
      static_cast<unsigned char>(a.a + static_cast<int>((b.a - a.a) * t)),
  };
}

static Color increase(const Color &color, const int factor = 10) {
  Color darkerColor;
  darkerColor.r = static_cast<unsigned char>(color.r + factor);
  darkerColor.g = static_cast<unsigned char>(color.g + factor);
  darkerColor.b = static_cast<unsigned char>(color.b + factor);
  darkerColor.a = color.a;
  return darkerColor;
}

static Color set_opacity(const Color &color, const unsigned char alpha) {
  Color transparentColor = color;
  transparentColor.a = alpha;
  return transparentColor;
}

static Color opacity_pct(const Color &color, const float percentage) {
  Color transparentColor = color;
  // Scale the alpha the colour already had, do not replace it: setting 255*pct
  // made a fully transparent fill become half-opaque at opacity 0.5, which is
  // how a disabled ghost button grew a background it never asked for.
  transparentColor.a = static_cast<unsigned char>(
      color.a * std::clamp(percentage, 0.0f, 1.0f));
  return transparentColor;
}

// Additional color utility functions
static Color get_opposite(const Color &color) {
  Color opposite;
  opposite.r = static_cast<unsigned char>(255 - color.r);
  opposite.g = static_cast<unsigned char>(255 - color.g);
  opposite.b = static_cast<unsigned char>(255 - color.b);
  opposite.a = color.a;
  return opposite;
}

static unsigned char comp_min(const Color &a) {
  return static_cast<unsigned char>(std::min({a.r, a.g, a.b}));
}

static unsigned char comp_max(const Color &a) {
  return static_cast<unsigned char>(std::max({a.r, a.g, a.b}));
}

static bool is_empty(const Color &c) {
  return c.r == 0 && c.g == 0 && c.b == 0 && c.a == 0;
}

// ============================================================
// HSV conversion and hex strings
// ============================================================

struct HSV {
  float h = 0.f; // degrees, 0..360
  float s = 0.f; // 0..1
  float v = 0.f; // 0..1
};

static HSV to_hsv(const Color &color) {
  const float r = color.r / 255.f;
  const float g = color.g / 255.f;
  const float b = color.b / 255.f;
  const float cmax = std::max({r, g, b});
  const float cmin = std::min({r, g, b});
  const float delta = cmax - cmin;

  HSV hsv{0.f, cmax == 0.f ? 0.f : delta / cmax, cmax};
  if (delta == 0.f)
    return hsv;
  if (cmax == r)
    hsv.h = 60.f * std::fmod((g - b) / delta, 6.f);
  else if (cmax == g)
    hsv.h = 60.f * ((b - r) / delta + 2.f);
  else
    hsv.h = 60.f * ((r - g) / delta + 4.f);
  hsv.h = std::fmod(hsv.h + 360.f, 360.f);
  return hsv;
}

static Color from_hsv(const HSV &hsv, unsigned char alpha = 255) {
  const float h = std::fmod(std::fmod(hsv.h, 360.f) + 360.f, 360.f);
  const float s = std::clamp(hsv.s, 0.f, 1.f);
  const float v = std::clamp(hsv.v, 0.f, 1.f);
  const float c = v * s;
  const float x = c * (1.f - std::abs(std::fmod(h / 60.f, 2.f) - 1.f));
  const float m = v - c;

  float r = 0.f, g = 0.f, b = 0.f;
  if (h < 60.f) { r = c; g = x; }
  else if (h < 120.f) { r = x; g = c; }
  else if (h < 180.f) { g = c; b = x; }
  else if (h < 240.f) { g = x; b = c; }
  else if (h < 300.f) { r = x; b = c; }
  else { r = c; b = x; }

  return Color{static_cast<unsigned char>(std::round((r + m) * 255.f)),
               static_cast<unsigned char>(std::round((g + m) * 255.f)),
               static_cast<unsigned char>(std::round((b + m) * 255.f)), alpha};
}

// "RRGGBB", or "RRGGBBAA" with include_alpha. No leading '#'.
static std::string to_hex_string(const Color &color,
                                 bool include_alpha = false) {
  static constexpr char digits[] = "0123456789ABCDEF";
  const unsigned char bytes[4] = {color.r, color.g, color.b, color.a};
  std::string out;
  const int count = include_alpha ? 4 : 3;
  out.reserve(static_cast<size_t>(count * 2));
  for (int i = 0; i < count; i++) {
    out += digits[bytes[i] >> 4];
    out += digits[bytes[i] & 0xF];
  }
  return out;
}

// Accepts an optional '#', then 3 ("36C" -> 33 66 CC), 6, or 8 hex digits.
static std::optional<Color> parse_hex(std::string_view text) {
  if (!text.empty() && text.front() == '#')
    text.remove_prefix(1);
  if (text.size() != 3 && text.size() != 6 && text.size() != 8)
    return std::nullopt;

  auto digit = [](char ch) -> int {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
  };
  unsigned char bytes[4] = {0, 0, 0, 255};
  if (text.size() == 3) {
    for (int i = 0; i < 3; i++) {
      const int d = digit(text[static_cast<size_t>(i)]);
      if (d < 0) return std::nullopt;
      bytes[i] = static_cast<unsigned char>(d * 16 + d);
    }
  } else {
    const int count = text.size() == 6 ? 3 : 4;
    for (int i = 0; i < count; i++) {
      const int hi = digit(text[static_cast<size_t>(i * 2)]);
      const int lo = digit(text[static_cast<size_t>(i * 2 + 1)]);
      if (hi < 0 || lo < 0) return std::nullopt;
      bytes[i] = static_cast<unsigned char>(hi * 16 + lo);
    }
  }
  return Color{bytes[0], bytes[1], bytes[2], bytes[3]};
}

// Accessibility and contrast utilities inspired by Aether's Garnish library
// https://github.com/Aeastr/Garnish

// ============================================================
// Luminance and Brightness (Garnish-inspired)
// ============================================================

// Helper: linearize a single sRGB component for luminance calculation
static float linearize_srgb_component(unsigned char component) {
  float c = static_cast<float>(component) / 255.0f;
  if (c <= 0.04045f) {
    return c / 12.92f;
  }
  return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Relative luminance per WCAG 2.1 (returns 0.0 to 1.0)
// Uses sRGB linearization as specified by WCAG
static float luminance(const Color &color) {
  float r = linearize_srgb_component(color.r);
  float g = linearize_srgb_component(color.g);
  float b = linearize_srgb_component(color.b);
  return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

// Perceived brightness (0.0 to 1.0) - simpler weighted average
// Formula: (0.299*R + 0.587*G + 0.114*B) / 255
static float brightness(const Color &color) {
  return (0.299f * static_cast<float>(color.r) +
          0.587f * static_cast<float>(color.g) +
          0.114f * static_cast<float>(color.b)) /
         255.0f;
}

// Classification: is the color perceived as light?
static bool is_light(const Color &color, float threshold = 0.5f) {
  return luminance(color) >= threshold;
}

// Classification: is the color perceived as dark?
static bool is_dark(const Color &color, float threshold = 0.5f) {
  return luminance(color) < threshold;
}

// ============================================================
// Contrast Ratio (WCAG 2.1)
// ============================================================

// Returns contrast ratio between two colors (1:1 to 21:1)
// Formula: (L1 + 0.05) / (L2 + 0.05) where L1 >= L2
static float contrast_ratio(const Color &foreground, const Color &background) {
  float l1 = luminance(foreground);
  float l2 = luminance(background);
  // Ensure l1 is the lighter luminance
  if (l1 < l2) {
    std::swap(l1, l2);
  }
  return (l1 + 0.05f) / (l2 + 0.05f);
}

// ============================================================
// WCAG Compliance Levels
// ============================================================

enum struct WCAGLevel {
  Fail,     // < 3:1
  AALarge,  // >= 3:1 (large text: 18pt+ or 14pt bold)
  AA,       // >= 4.5:1 (normal text)
  AAALarge, // >= 4.5:1 (large text enhanced)
  AAA       // >= 7:1 (normal text enhanced)
};

// Determine WCAG compliance level for a color pair
static WCAGLevel wcag_compliance(const Color &foreground,
                                 const Color &background) {
  float ratio = contrast_ratio(foreground, background);
  if (ratio >= 7.0f) {
    return WCAGLevel::AAA;
  }
  if (ratio >= 4.5f) {
    return WCAGLevel::AA;
  }
  if (ratio >= 3.0f) {
    return WCAGLevel::AALarge;
  }
  return WCAGLevel::Fail;
}

// Check if color pair meets WCAG AA (4.5:1 for normal text)
static bool meets_wcag_aa(const Color &foreground, const Color &background) {
  return contrast_ratio(foreground, background) >= 4.5f;
}

// Check if color pair meets WCAG AAA (7:1 for normal text)
static bool meets_wcag_aaa(const Color &foreground, const Color &background) {
  return contrast_ratio(foreground, background) >= 7.0f;
}

// ============================================================
// Auto-Contrast Text Generation (Core Garnish Feature)
// ============================================================

// Monochromatic: Returns white or black text based on background luminance
static Color auto_text_color(const Color &background) {
  // Use luminance threshold of ~0.179 (W3C recommendation for text)
  // This is derived from the contrast ratio formula
  if (luminance(background) > 0.179f) {
    return UI_BLACK;
  }
  return UI_WHITE;
}

// Bi-chromatic: Returns one of two provided colors for best contrast
// Falls back to pure white/black if neither option achieves minimum contrast
static Color auto_text_color(const Color &background, const Color &light_option,
                             const Color &dark_option,
                             float min_contrast = 4.5f) {
  float light_contrast = contrast_ratio(light_option, background);
  float dark_contrast = contrast_ratio(dark_option, background);

  // Pick the better of the two provided options
  Color best_option =
      (light_contrast > dark_contrast) ? light_option : dark_option;
  float best_contrast = std::max(light_contrast, dark_contrast);

  // If neither achieves minimum contrast, fall back to pure white/black
  if (best_contrast < min_contrast) {
    float white_contrast = contrast_ratio(UI_WHITE, background);
    float black_contrast = contrast_ratio(UI_BLACK, background);
    return (white_contrast > black_contrast) ? UI_WHITE : UI_BLACK;
  }

  return best_option;
}

// ============================================================
// Color Harmony Utilities
// ============================================================

// Mix two colors with weighted blend (weight 0.0 = all a, 1.0 = all b)
static Color mix(const Color &a, const Color &b, float weight = 0.5f) {
  weight = std::clamp(weight, 0.0f, 1.0f);
  float inv_weight = 1.0f - weight;
  return Color{
      static_cast<unsigned char>(a.r * inv_weight + b.r * weight),
      static_cast<unsigned char>(a.g * inv_weight + b.g * weight),
      static_cast<unsigned char>(a.b * inv_weight + b.b * weight),
      static_cast<unsigned char>(a.a * inv_weight + b.a * weight),
  };
}

// Adjust color luminance to achieve target contrast against background
static Color ensure_contrast(const Color &color, const Color &background,
                             float min_contrast = 4.5f) {
  float current = contrast_ratio(color, background);
  if (current >= min_contrast) {
    return color; // Already meets requirement
  }

  // Determine if we should lighten or darken the color
  float bg_lum = luminance(background);
  bool should_lighten = bg_lum < 0.5f;

  // Binary search for the right adjustment amount
  Color result = color;
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 16; ++i) { // 16 iterations for precision
    float mid = (lo + hi) / 2.0f;
    Color candidate =
        should_lighten ? lighten(color, mid) : darken(color, 1.0f - mid);
    float ratio = contrast_ratio(candidate, background);
    if (ratio >= min_contrast) {
      result = candidate;
      hi = mid; // Try less adjustment
    } else {
      lo = mid; // Need more adjustment
    }
  }
  return result;
}

// Generate an optimal contrasting shade of the input color
static Color contrasting_shade(const Color &color,
                               float target_contrast = 4.5f) {
  // Create a contrasting version by adjusting luminance significantly
  float lum = luminance(color);
  if (lum > 0.5f) {
    // Color is light, create a dark shade
    return ensure_contrast(darken(color, 0.3f), color, target_contrast);
  } else {
    // Color is dark, create a light shade
    return ensure_contrast(lighten(color, 0.5f), color, target_contrast);
  }
}

// ============================================================
// Font Weight Optimization
// ============================================================

enum struct FontWeight {
  Light = 300,
  Regular = 400,
  Medium = 500,
  SemiBold = 600,
  Bold = 700
};

// Suggests minimum font weight for readability given contrast ratio
// Lower contrast requires bolder fonts for legibility
static FontWeight suggested_font_weight(const Color &foreground,
                                        const Color &background) {
  float ratio = contrast_ratio(foreground, background);
  if (ratio >= 7.0f) {
    return FontWeight::Light; // High contrast, any weight works
  }
  if (ratio >= 4.5f) {
    return FontWeight::Regular; // AA compliant, normal weight
  }
  if (ratio >= 3.0f) {
    return FontWeight::Medium; // Large text threshold, bump weight
  }
  // Below AA, recommend bolder fonts for readability
  return FontWeight::Bold;
}

} // namespace colors

struct HasColor : BaseComponent {
private:
  mutable Color color_;

public:
  using FetchFn = std::function<Color()>;
  bool is_dynamic = false;
  FetchFn fetch_fn = nullptr;
  bool skip_hover_override = false;
  std::optional<Color> hover_color; // Per-component hover background override

  HasColor(const Color c) : color_(c) {}
  HasColor(const FetchFn &fetch)
      : color_(fetch()), is_dynamic(true), fetch_fn(fetch) {}

  Color color() const {
    if (fetch_fn) {
      color_ = fetch_fn();
    }
    return color_;
  }

  // Hover background: explicit override if set, else a luminance-derived tint
  // of the widget's own color (dark -> lighter, light -> darker). Avoids the
  // old default of flashing to the theme Background on hover.
  Color hover_bg() const {
    if (hover_color.has_value())
      return hover_color.value();
    const Color base = color();
    return colors::is_dark(base) ? colors::lighten(base, 0.15f)
                                 : colors::darken(base, 0.85f);
  }
  auto &set(const Color col) {
    color_ = col;
    return *this;
  }
};
} // namespace afterhours
