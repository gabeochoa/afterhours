#pragma once

#include <ostream>
#include <string_view>

#include "../../logging.h" // defines AFTER_HOURS_HAS_FORMAT

// std::formatter specializations below need <format>; on toolchains without it
// (gcc 11/12 etc.) logging is no-op anyway, so skip them. See logging.h.
#if AFTER_HOURS_HAS_FORMAT
#include <format>
#endif

namespace afterhours {

namespace ui {

/// Controls how pixel values are resolved during layout.
///
/// - Proportional: pixels() = fixed hardware pixels. h720()/screen_pct() scale
///   with resolution. Good for games rendering to a reference resolution.
///
/// - Adaptive: pixels() = logical pixels scaled by ui_scale. Resolution changes
///   reflow layout without changing element sizes. Like how the web works:
///   Ctrl+/- changes ui_scale (zoom), window resize changes available space.
enum class ScalingMode {
  Proportional, // Current/default. Resolution scales everything.
  Adaptive, // Web-like. ui_scale controls element sizes, resolution reflows.
};

enum struct Dim {
  None,
  Pixels,
  Text,
  Percent,
  Children,
  ScreenPercent,
  Expand, // Fill remaining space proportionally by weight (like CSS flex-grow)
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
  case Dim::Expand:
    os << "Expand";
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
#if AFTER_HOURS_HAS_FORMAT
namespace std {
template <> struct formatter<afterhours::ui::Dim> {
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
    case afterhours::ui::Dim::Expand:
      name = "Expand";
      break;
    }
    return std::format_to(ctx.out(), "{}", name);
  }
};

template <> struct formatter<afterhours::ui::Size> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  auto format(const afterhours::ui::Size &size,
              std::format_context &ctx) const {
    return std::format_to(ctx.out(), "Size(dim: {}, value: {}, strictness: {})",
                          size.dim, size.value, size.strictness);
  }
};
} // namespace std
#endif // AFTER_HOURS_HAS_FORMAT

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

/// Expand to fill remaining space proportionally by weight.
/// Like CSS flex-grow: elements with expand(2) get twice the space of
/// expand(1). Weight must be positive (default 1.0).
inline Size expand(const float weight = 1.f) {
  if (weight <= 0.f) {
    log_warn("Expand weight must be positive, got {}", weight);
  }
  return ui::Size{.dim = ui::Dim::Expand, .value = weight, .strictness = 0.f};
}

/// Alias for expand() matching CSS flex-grow terminology.
inline Size flex_grow(const float weight = 1.f) { return expand(weight); }

inline Size h720(const float px) { return screen_pct(px / 720.f); }
inline Size w1280(const float px) { return screen_pct(px / 1280.f); }

// Resolve a Size to pixels given a screen dimension (height for h720, width
// for w1280). Does NOT apply ui_scale — use the overload with ScalingMode for
// that.
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
  case Dim::Expand:
    log_warn("Cannot resolve dim {} to pixels - using raw value",
             static_cast<int>(size.dim));
    return size.value;
  }
  return size.value;
}

// Resolve a Size to pixels, applying ui_scale in Adaptive mode.
// Use this overload when resolving sizes that should respect the scaling mode
// (e.g., translate/absolute position values, font sizes for rendering).
inline float resolve_to_pixels(const Size &size, float screen_dimension,
                               ScalingMode mode, float ui_scale) {
  switch (size.dim) {
  case Dim::Pixels:
    if (mode == ScalingMode::Adaptive) {
      return size.value * ui_scale;
    }
    return size.value;
  case Dim::ScreenPercent:
    return size.value * screen_dimension;
  case Dim::Percent:
  case Dim::Children:
  case Dim::Text:
  case Dim::None:
  case Dim::Expand:
    log_warn("Cannot resolve dim {} to pixels - using raw value",
             static_cast<int>(size.dim));
    return size.value;
  }
  return size.value;
}

// Shorthand spelling of the 8pt grid. Each step is an alias for the
// DefaultSpacing method of the same rank, so there is one scale rather than
// two that disagree.
enum struct Spacing {
  xs, // tiny:      8px at 720p
  sm, // small:    16px
  md, // medium:   24px
  lg, // large:    32px
  xl, // xlarge:   48px
};

// Defined in styling_defaults.h, which is where DefaultSpacing lives; that
// header includes this one, so the body cannot be written here.
Size spacing_to_size(Spacing spacing);

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

  auto scale_x(float s) {
    if ((x_axis.dim == ui::Dim::Children && x_axis.value < 0) ||
        x_axis.dim == ui::Dim::Text || x_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               static_cast<int>(x_axis.dim));
    }
    x_axis.value *= s;
    return ComponentSize(*this);
  }
  auto scale_y(float s) {
    if ((y_axis.dim == ui::Dim::Children && y_axis.value < 0) ||
        y_axis.dim == ui::Dim::Text || y_axis.dim == ui::Dim::None) {
      log_warn("Scaling component size with dim {} may be unsupported",
               static_cast<int>(y_axis.dim));
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
    log_warn("half size not supported for dim {}", static_cast<int>(size.dim));
  } break;
  case Dim::ScreenPercent:
  case Dim::Percent:
  case Dim::Pixels:
  case Dim::Expand:
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

/// Controls whether children wrap to new rows/columns when they exceed
/// container size. NoWrap prevents wrapping and generates warnings when
/// overflow would occur.
enum struct FlexWrap {
  Wrap,   // Allow wrapping to new row/column
  NoWrap, // Never wrap - overflow/clip instead. The default, unlike CSS.
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

// Side names are capitalized because a static `left()` would collide with the
// `left` field. Prefer the designated form -- Padding{.left = pixels(10)} --
// which reads the same either way.
struct Padding {
  Size top;
  Size left;
  Size bottom;
  Size right;

  static constexpr Padding all(Size v) { return {v, v, v, v}; }
  static constexpr Padding vertical(Size v) { return {v, {}, v, {}}; }
  static constexpr Padding horizontal(Size v) { return {{}, v, {}, v}; }
  static constexpr Padding Top(Size v) { return {v, {}, {}, {}}; }
  static constexpr Padding Left(Size v) { return {{}, v, {}, {}}; }
  static constexpr Padding Bottom(Size v) { return {{}, {}, v, {}}; }
  static constexpr Padding Right(Size v) { return {{}, {}, {}, v}; }
};

// Same field order as Padding on purpose: the two are interchangeable at most
// call sites, and a positional {a, b, c, d} that means different things in each
// is a mistake nothing would catch.
struct Margin {
  Size top;
  Size left;
  Size bottom;
  Size right;

  static constexpr Margin all(Size v) { return {v, v, v, v}; }
  static constexpr Margin vertical(Size v) { return {v, {}, v, {}}; }
  static constexpr Margin horizontal(Size v) { return {{}, v, {}, v}; }
  static constexpr Margin Top(Size v) { return {v, {}, {}, {}}; }
  static constexpr Margin Left(Size v) { return {{}, v, {}, {}}; }
  static constexpr Margin Bottom(Size v) { return {{}, {}, v, {}}; }
  static constexpr Margin Right(Size v) { return {{}, {}, {}, v}; }
};

// Locks each factory to its side, so the order above cannot drift again.
namespace detail {
constexpr Size kProbe{Dim::Pixels, 7.f, 1.f};
constexpr bool set(Size s) { return s.dim == Dim::Pixels; }

static_assert(set(Padding::Top(kProbe).top) && set(Margin::Top(kProbe).top));
static_assert(set(Padding::Left(kProbe).left) && set(Margin::Left(kProbe).left));
static_assert(set(Padding::Bottom(kProbe).bottom) &&
              set(Margin::Bottom(kProbe).bottom));
static_assert(set(Padding::Right(kProbe).right) &&
              set(Margin::Right(kProbe).right));

constexpr bool only_vertical(auto p) {
  return set(p.top) && set(p.bottom) && !set(p.left) && !set(p.right);
}
constexpr bool only_horizontal(auto p) {
  return set(p.left) && set(p.right) && !set(p.top) && !set(p.bottom);
}
static_assert(only_vertical(Padding::vertical(kProbe)) &&
              only_vertical(Margin::vertical(kProbe)));
static_assert(only_horizontal(Padding::horizontal(kProbe)) &&
              only_horizontal(Margin::horizontal(kProbe)));
} // namespace detail

/// Breakpoint helper for responsive layout decisions.
///
/// In Adaptive mode, the "logical" screen size is screen_size / ui_scale,
/// which represents how much layout space is available (like CSS viewport
/// width when browser is zoomed). Screens can use this to switch layouts:
///
///   auto info = LayoutInfo::from_context(ctx);
///   if (info.is_narrow()) { /* stack vertically */ }
///   else { /* side by side */ }
///
struct LayoutInfo {
  float logical_w = 1280.f;
  float logical_h = 720.f;
  float ui_scale = 1.0f;
  ScalingMode mode = ScalingMode::Proportional;

  /// Create from screen dimensions and theme settings.
  static LayoutInfo make(float screen_w, float screen_h, float ui_scale_,
                         ScalingMode mode_) {
    LayoutInfo info;
    info.ui_scale = ui_scale_;
    info.mode = mode_;
    if (mode_ == ScalingMode::Adaptive && ui_scale_ > 0.f) {
      info.logical_w = screen_w / ui_scale_;
      info.logical_h = screen_h / ui_scale_;
    } else {
      info.logical_w = screen_w;
      info.logical_h = screen_h;
    }
    return info;
  }

  // Common breakpoints (logical pixels)
  bool is_narrow() const { return logical_w < 800.f; }
  bool is_medium() const { return logical_w >= 800.f && logical_w < 1200.f; }
  bool is_wide() const { return logical_w >= 1200.f; }
  bool is_short() const { return logical_h < 600.f; }
};

} // namespace ui

} // namespace afterhours

// std::formatter specialization for Axis to support std::format/log_* macros
#if AFTER_HOURS_HAS_FORMAT
namespace std {
template <> struct formatter<afterhours::ui::Axis> {
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
#endif // AFTER_HOURS_HAS_FORMAT
