
#pragma once

#include <algorithm>
#include <cmath>

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
static const Color red = UI_RED;
static const Color transleucent_green = Color{0, 250, 50, 5};
static const Color transleucent_red = Color{250, 0, 50, 5};
static const Color pacific_blue = Color{71, 168, 189, 255};
static const Color oxford_blue = Color{12, 27, 51, 255};
static const Color orange_soda = Color{240, 100, 73, 255};
static const Color isabelline = Color{237, 230, 227, 255};
static const Color tea_green =
    Color{166, 185, 189, 255}; // 195, 232, 189, 255};

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
  transparentColor.a =
      static_cast<unsigned char>(255 * std::clamp(percentage, 0.0f, 1.0f));
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

#ifdef AFTER_HOURS_USE_RAYLIB
// HSL conversion functions (requires Vector3 from raylib)
static raylib::Vector3 to_hsl(const Color color) {
  const unsigned char cmax = comp_max(color);
  const unsigned char cmin = comp_min(color);
  const unsigned char delta = cmax - cmin;

  constexpr float COLOR_EPSILON = 0.000001f;
  raylib::Vector3 hsl = {0.f, 0.f, (cmax + cmin) / 2.f};

  if (std::abs(static_cast<float>(delta)) <= COLOR_EPSILON) {
    return hsl;
  }

  if (std::abs(static_cast<float>(cmax - color.r)) <= COLOR_EPSILON) {
    hsl.x = static_cast<float>(
        std::fmod(static_cast<float>(color.g - color.b) / delta, 6.f));
  } else if (std::abs(static_cast<float>(cmax - color.g)) <= COLOR_EPSILON) {
    hsl.x = (static_cast<float>(color.b - color.r) / delta) + 2.f;
  } else {
    hsl.x = (static_cast<float>(color.r - color.g) / delta) + 4.f;
  }

  hsl.y = static_cast<float>(delta / (1.f - std::abs(2.f * hsl.z - 1.f)));
  hsl.x /= 6.f;
  hsl.x = std::fmod(hsl.x + 1.f, 1.f);
  return hsl;
}

static Color to_rgb(const raylib::Vector3 &hsl) {
  const float k = hsl.x * 6.f;
  const float c = (1.f - std::abs(2.f * hsl.z - 1.f)) * hsl.y;
  const float x = c * (1.f - std::abs(std::fmod(k, 2.f) - 1.f));
  const float m = hsl.z - c / 2.f;
  const int d = static_cast<int>(std::floor(k));

  raylib::Vector3 rgb;

  switch (d) {
  case 0:
    rgb = {c, x, 0.f};
    break;
  case 1:
    rgb = {x, c, 0.f};
    break;
  case 2:
    rgb = {0.f, c, x};
    break;
  case 3:
    rgb = {0.f, x, c};
    break;
  case 4:
    rgb = {x, 0.f, c};
    break;
  default:
    rgb = {c, 0.f, x};
    break;
  }

  return Color{
      static_cast<unsigned char>(255 * (rgb.x + m)),
      static_cast<unsigned char>(255 * (rgb.y + m)),
      static_cast<unsigned char>(255 * (rgb.z + m)),
      static_cast<unsigned char>(255),
  };
}

static Color get_highlighted(const Color &color) {
  auto hsl = to_hsl(color);
  hsl.z = (hsl.z + 0.01f);
  return to_rgb(hsl);
}
#else
// Non-raylib versions - would need Vector3Type defined
// For now, these are stubs that return default values
struct Vector3Type {
  float x, y, z;
};

static Vector3Type to_hsl(const Color) {
  log_warn("HSL conversion not supported without raylib");
  return Vector3Type{0, 0, 0};
}

static Color to_rgb(const Vector3Type &) {
  log_warn("RGB conversion not supported without raylib");
  return Color{0, 0, 0, 255};
}

static Color get_highlighted(const Color &color) {
  log_warn("get_highlighted not supported without raylib");
  return color;
}
#endif

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

  HasColor(const Color c) : color_(c) {}
  HasColor(const FetchFn &fetch)
      : color_(fetch()), is_dynamic(true), fetch_fn(fetch) {}

  Color color() const {
    if (fetch_fn) {
      color_ = fetch_fn();
    }
    return color_;
  }
  auto &set(const Color col) {
    color_ = col;
    return *this;
  }
};
} // namespace afterhours
