#pragma once

#include <format>
#include <iostream>
#include <string_view>

#include "../../logging.h"

namespace afterhours {

namespace ui {

enum struct Dim {
  None,
  Pixels,
  Text,
  Percent,
  Children,
  ScreenPercent,
};

inline std::ostream &operator<<(std::ostream &os, const Dim &dim) {
  switch (dim) {
  case Dim::None:
    os << "None";
    break;
  case Dim::Pixels:
    os << "Pixels";
    break;
  case Dim::Text:
    os << "Text";
    break;
  case Dim::Percent:
    os << "Percent";
    break;
  case Dim::Children:
    os << "Children";
    break;
  case Dim::ScreenPercent:
    os << "ScreenPercent";
    break;
  }
  return os;
}

struct Size {
  Dim dim = Dim::None;
  float value = -1;
  float strictness = 1.f;
};

inline std::ostream &operator<<(std::ostream &os, const Size &size) {
  os << "Size(dim: " << size.dim << ", value: " << size.value
     << ", strictness: " << size.strictness << ")";
  return os;
}

} // namespace ui
} // namespace afterhours

// Define formatters for Dim and Size BEFORE they are used in log statements
namespace std {
template <>
struct formatter<afterhours::ui::Dim> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(afterhours::ui::Dim dim, std::format_context &ctx) const {
    std::string_view name = "Unknown";
    switch (dim) {
    case afterhours::ui::Dim::None:
      name = "None";
      break;
    case afterhours::ui::Dim::Pixels:
      name = "Pixels";
      break;
    case afterhours::ui::Dim::Text:
      name = "Text";
      break;
    case afterhours::ui::Dim::Percent:
      name = "Percent";
      break;
    case afterhours::ui::Dim::Children:
      name = "Children";
      break;
    case afterhours::ui::Dim::ScreenPercent:
      name = "ScreenPercent";
      break;
    }
    return std::format_to(ctx.out(), "{}", name);
  }
};

template <>
struct formatter<afterhours::ui::Size> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(const afterhours::ui::Size &size,
              std::format_context &ctx) const {
    return std::format_to(ctx.out(), "Size(dim: {}, value: {}, strictness: {})",
                          size.dim, size.value, size.strictness);
  }
};
} // namespace std

// Reopen namespace to continue with functions that use logging
namespace afterhours {
namespace ui {

inline Size pixels(const float value, const float strictness = 1.f) {
  return ui::Size{
      .dim = ui::Dim::Pixels, .value = value, .strictness = strictness};
}

inline Size percent(const float value, const float strictness = 1.f) {
  if (value > 1.f) {
    log_warn("Value should be between 0 and 1");
  }
  return ui::Size{
      .dim = ui::Dim::Percent, .value = value, .strictness = strictness};
}

inline Size screen_pct(const float value, const float strictness = 0.9f) {
  if (value > 1.f) {
    log_warn("Value should be between 0 and 1");
  }
  return ui::Size{
      .dim = ui::Dim::ScreenPercent, .value = value, .strictness = strictness};
}

inline Size children(const float value = -1) {
  return ui::Size{.dim = ui::Dim::Children, .value = value};
}

inline Size h720(const float px) { return screen_pct(px / 720.f); }
inline Size w1280(const float px) { return screen_pct(px / 1280.f); }

// Resolve a Size to pixels given a screen dimension (height for h720, width
// for w1280)
inline float resolve_to_pixels(const Size &size, float screen_dimension) {
  switch (size.dim) {
  case Dim::Pixels:
    return size.value;
  case Dim::ScreenPercent:
    return size.value * screen_dimension;
  case Dim::Percent:
  case Dim::Children:
  case Dim::Text:
  case Dim::None:
    // For these types, just return the raw value as a fallback
    // In practice, Percent/Children/Text should only be used for layout
    log_warn("Cannot resolve {} to pixels - using raw value", size);
    return size.value;
  }
  return size.value;
}

enum struct Spacing {
  xs, // Extra small: 0.01f (7.2px at 720p)
  sm, // Small: 0.02f (14.4px at 720p)
  md, // Medium: 0.04f (28.8px at 720p)
  lg, // Large: 0.08f (57.6px at 720p)
  xl, // Extra large: 0.16f (115.2px at 720p)
};

inline Size spacing_to_size(Spacing spacing) {
  switch (spacing) {
  case Spacing::xs:
    return screen_pct(0.01f);
  case Spacing::sm:
    return screen_pct(0.02f);
  case Spacing::md:
    return screen_pct(0.04f);
  case Spacing::lg:
    return screen_pct(0.08f);
  case Spacing::xl:
    return screen_pct(0.16f);
  }
  return screen_pct(0.04f); // Default to medium
}

struct ComponentSize {
  Size x_axis;
  Size y_axis;

  bool is_default = false;
  ComponentSize(std::pair<Size, Size> pair)
      : x_axis(pair.first), y_axis(pair.second) {}
  ComponentSize(Size x, Size y) : x_axis(x), y_axis(y) {}
  ComponentSize(Size x, Size y, bool is_default_)
      : x_axis(x), y_axis(y), is_default(is_default_) {}
  ComponentSize(std::pair<Size, Size> pair, bool is_default_)
      : x_axis(pair.first), y_axis(pair.second), is_default(is_default_) {}

  auto _scale_x(float s) {
    if ((x_axis.dim == ui::Dim::Children && x_axis.value < 0) ||
        x_axis.dim == ui::Dim::Text || x_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               x_axis.dim);
    }
    x_axis.value *= s;
    return ComponentSize(*this);
  }
  auto _scale_y(float s) {
    if ((y_axis.dim == ui::Dim::Children && y_axis.value < 0) ||
        y_axis.dim == ui::Dim::Text || y_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               y_axis.dim);
    }
    y_axis.value *= s;
    return ComponentSize(*this);
  }
};

inline std::ostream &operator<<(std::ostream &os, const ComponentSize &cs) {
  os << "ComponentSize(x: " << cs.x_axis << ", y: " << cs.y_axis << ")";
  return os;
}

inline ComponentSize pixels_xy(float width, float height) {
  return {pixels(width), pixels(height)};
}

inline ComponentSize children_xy() {
  return {
      children(),
      children(),
  };
}

inline Size half_size(Size size) {
  switch (size.dim) {
  case Dim::Children:
  case Dim::Text:
  case Dim::None: {
    log_warn("half size not supported for dim {}", size.dim);
  } break;
  case Dim::ScreenPercent:
  case Dim::Percent:
  case Dim::Pixels:
    return Size{
        .dim = size.dim,
        .value = size.value / 2.f,
        .strictness = size.strictness,
    };
  }
  return size;
}

enum struct FlexDirection {
  None = 1 << 0,
  Row = 1 << 1,
  Column = 1 << 2,
};

/// Controls how children are distributed along the main axis (flex direction).
/// Default: FlexStart preserves current behavior (pack items at start).
enum struct JustifyContent {
  FlexStart,    // Pack items at start (default - current behavior)
  FlexEnd,      // Pack items at end
  Center,       // Center items
  SpaceBetween, // Distribute space between items (first and last at edges)
  SpaceAround,  // Distribute space around items (equal space on both sides)
};

/// Controls how children are aligned along the cross axis.
/// Default: FlexStart preserves current behavior.
enum struct AlignItems {
  FlexStart, // Align to start of cross axis (default - current behavior)
  FlexEnd,   // Align to end of cross axis
  Center,    // Center on cross axis
  Stretch,   // Stretch to fill (for items without explicit cross-axis size)
};

/// Controls how an individual element aligns itself within its parent.
/// Overrides the parent's align_items for this specific element.
/// Auto means inherit from parent's align_items setting.
enum struct SelfAlign {
  Auto,      // Inherit from parent's align_items (default)
  FlexStart, // Align to start of cross axis
  FlexEnd,   // Align to end of cross axis
  Center,    // Center on cross axis - common for centering content containers
};

/// Controls whether children wrap to new rows/columns when they exceed container size.
/// NoWrap prevents wrapping and generates warnings when overflow would occur.
enum struct FlexWrap {
  Wrap,   // Allow wrapping to new row/column (default behavior)
  NoWrap, // Never wrap - overflow/clip instead, warn if would overflow
};

enum struct Axis {
  X = 0,
  Y = 1,

  left = 2,
  top = 3,

  right = 4,
  bottom = 5,
};

inline std::ostream &operator<<(std::ostream &os, const Axis &axis) {
  switch (axis) {
  case Axis::X:
    os << "X-Axis";
    break;
  case Axis::Y:
    os << "Y-Axis";
    break;
  case Axis::left:
    os << "left";
    break;
  case Axis::right:
    os << "right";
    break;
  case Axis::top:
    os << "top";
    break;
  case Axis::bottom:
    os << "bottom";
    break;
  }
  return os;
}

struct Padding {
  Size top;
  Size left;
  Size bottom;
  Size right;
};

struct Margin {
  Size top;
  Size bottom;
  Size left;
  Size right;
};

} // namespace ui

} // namespace afterhours

// std::formatter specialization for Axis to support std::format/log_* macros
namespace std {
template <>
struct formatter<afterhours::ui::Axis> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(afterhours::ui::Axis axis, std::format_context &ctx) const {
    std::string_view name = "Unknown";
    switch (axis) {
    case afterhours::ui::Axis::X:
      name = "X-Axis";
      break;
    case afterhours::ui::Axis::Y:
      name = "Y-Axis";
      break;
    case afterhours::ui::Axis::left:
      name = "left";
      break;
    case afterhours::ui::Axis::top:
      name = "top";
      break;
    case afterhours::ui::Axis::right:
      name = "right";
      break;
    case afterhours::ui::Axis::bottom:
      name = "bottom";
      break;
    }
    return std::format_to(ctx.out(), "{}", name);
  }
};
} // namespace std
