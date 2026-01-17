// Bevel Border Plugin for Afterhours
// Provides Win95/retro-style 3D beveled borders for UI elements.
//
// To integrate into Afterhours:
// 1. Add BevelStyle enum and BevelBorder struct to components.h
// 2. Add HasBevelBorder component
// 3. Add with_bevel() to ComponentConfig
// 4. Add bevel rendering in rendering.h after regular border rendering

#pragma once

#include <afterhours/src/plugins/color.h>

namespace afterhours {
namespace ui {

/// Bevel style for 3D border effects
enum class BevelStyle {
  None,    // No bevel (flat border)
  Raised,  // Light on top-left, dark on bottom-right (buttons, panels)
  Sunken,  // Dark on top-left, light on bottom-right (inputs, wells)
};

/// Bevel border configuration
struct BevelBorder {
  Color light_color = {255, 255, 255, 255};  // Top-left edges
  Color dark_color = {128, 128, 128, 255};   // Bottom-right edges
  float thickness = 1.0f;
  BevelStyle style = BevelStyle::Raised;

  bool has_bevel() const { 
    return style != BevelStyle::None && thickness > 0.0f; 
  }
  
  // Factory methods for common use cases
  static BevelBorder raised(float t = 1.0f) {
    return BevelBorder{{255, 255, 255, 255}, {128, 128, 128, 255}, t, BevelStyle::Raised};
  }
  
  static BevelBorder sunken(float t = 1.0f) {
    return BevelBorder{{255, 255, 255, 255}, {128, 128, 128, 255}, t, BevelStyle::Sunken};
  }
  
  static BevelBorder with_colors(Color light, Color dark, BevelStyle style = BevelStyle::Raised) {
    return BevelBorder{light, dark, 1.0f, style};
  }
};

/// Component for entities with bevel borders
struct HasBevelBorder : BaseComponent {
  BevelBorder bevel;
  HasBevelBorder() = default;
  explicit HasBevelBorder(const BevelBorder& b) : bevel(b) {}
};

//-----------------------------------------------------------------------------
// Rendering helper - call this in your render system
//-----------------------------------------------------------------------------

/// Draw a bevel border around a rectangle
/// Call this after drawing the element's background, before drawing content
template<typename DrawLineFn>
inline void draw_bevel_border(RectangleType rect, const BevelBorder& bevel, 
                               DrawLineFn draw_line) {
  if (!bevel.has_bevel()) return;
  
  int x = static_cast<int>(rect.x);
  int y = static_cast<int>(rect.y);
  int w = static_cast<int>(rect.width);
  int h = static_cast<int>(rect.height);
  int thickness = static_cast<int>(bevel.thickness);
  
  Color top_left, bottom_right;
  if (bevel.style == BevelStyle::Raised) {
    top_left = bevel.light_color;
    bottom_right = bevel.dark_color;
  } else {
    top_left = bevel.dark_color;
    bottom_right = bevel.light_color;
  }
  
  for (int i = 0; i < thickness; ++i) {
    // Top edge
    draw_line(x + i, y + i, x + w - i - 1, y + i, top_left);
    // Left edge
    draw_line(x + i, y + i, x + i, y + h - i - 1, top_left);
    // Bottom edge
    draw_line(x + i, y + h - i - 1, x + w - i, y + h - i - 1, bottom_right);
    // Right edge
    draw_line(x + w - i - 1, y + i, x + w - i - 1, y + h - i, bottom_right);
  }
}

#ifdef AFTER_HOURS_USE_RAYLIB
/// Convenience overload using raylib directly
inline void draw_bevel_border(RectangleType rect, const BevelBorder& bevel) {
  draw_bevel_border(rect, bevel, [](int x1, int y1, int x2, int y2, Color c) {
    raylib::DrawLine(x1, y1, x2, y2, raylib::Color{c.r, c.g, c.b, c.a});
  });
}
#endif

//-----------------------------------------------------------------------------
// ComponentConfig extension (add these methods to ComponentConfig)
//-----------------------------------------------------------------------------

/*
// Add to ComponentConfig class:

std::optional<BevelBorder> bevel_config;

ComponentConfig& with_bevel(BevelStyle style, 
                            Color light = {255, 255, 255, 255},
                            Color dark = {128, 128, 128, 255},
                            float thickness = 1.0f) {
  bevel_config = BevelBorder{light, dark, thickness, style};
  return *this;
}

ComponentConfig& with_raised_bevel() {
  return with_bevel(BevelStyle::Raised);
}

ComponentConfig& with_sunken_bevel() {
  return with_bevel(BevelStyle::Sunken);
}

bool has_bevel() const { return bevel_config.has_value(); }
*/

} // namespace ui
} // namespace afterhours

