#pragma once

// How big a config would come out, without building it.
//
// Windowing a variable-height list needs a prefix sum over the real heights,
// and getting those used to mean building every item -- which is the cost the
// windowing exists to avoid. So every consumer restated the box model in app
// code ("margin-top plus body plus margin-bottom") and silently went wrong the
// day the library summed a column differently.

// context.h first: styling_defaults.h, pulled in below, names UIContext.
#include "context.h"

#include "component_config.h"
#include "layout_types.h"
#include "text_measure.h"
#include "theme.h"

namespace afterhours {
namespace ui {
namespace imm {

struct MeasuredConfig {
  Vector2Type size;   // the box itself, padding in, margin out
  Vector2Type margin; // total on each axis
  // What a column or row advances by per item.
  Vector2Type pitch() const {
    return Vector2Type{size.x + margin.x, size.y + margin.y};
  }
};

namespace detail {

inline float measure_screen_dim(Axis axis) {
  auto *res = EntityHelper::get_singleton_cmp<
      window_manager::ProvidesCurrentResolution>();
  if (!res)
    return axis == Axis::X ? 1280.f : 720.f;
  return axis == Axis::X ? static_cast<float>(res->width())
                         : static_cast<float>(res->height());
}

// Padding and margin are Sizes too, and percent ones resolve against the space
// the caller says is available.
inline float measure_edge(const Size &s, float available, float screen_dim) {
  switch (s.dim) {
  case Dim::Pixels:
    return s.value;
  case Dim::ScreenPercent:
    return s.value * screen_dim;
  case Dim::Percent:
    return s.value * available;
  case Dim::Text:
  case Dim::Children:
  case Dim::Expand:
  case Dim::None:
    return 0.f;
  }
  return 0.f;
}

} // namespace detail

/// The size `config` would resolve to inside `available_w` x `available_h`.
///
/// Answers the dimensions that need no tree: Pixels, ScreenPercent, Percent of
/// what you pass in, and Text measured from the config's own label. Children
/// and Expand are answers about siblings that do not exist yet, so they come
/// back as 0 -- measure those by measuring what goes in them.
inline MeasuredConfig measure_config(const ComponentConfig &config,
                                     float available_w,
                                     float available_h = 0.f) {
  const Theme &theme = ThemeDefaults::get().theme;
  const float screen_w = detail::measure_screen_dim(Axis::X);
  const float screen_h = detail::measure_screen_dim(Axis::Y);

  const float font_px =
      resolve_to_pixels(config.font_size, screen_h, ScalingMode::Adaptive,
                        theme.ui_scale);
  // The renderer reserves this inside the box, and Dim::Text charges for it,
  // so a measurement that skipped it would be short by both sides.
  const Vector2Type inset = config.text_inset.value_or(theme.text_inset);
  const float inset_x = inset.x * theme.ui_scale * 2.f;

  const auto axis_size = [&](const Size &s, float available, float screen_dim,
                             bool is_x) -> float {
    switch (s.dim) {
    case Dim::Pixels:
      return s.value;
    case Dim::ScreenPercent:
      return s.value * screen_dim;
    case Dim::Percent:
      return s.value * available;
    case Dim::Text: {
      if (config.label.empty())
        return 0.f;
      // Wrapping only happens where the renderer would also wrap: a pinned
      // size and a width to wrap against.
      const bool wraps = config.text_overflow == TextOverflow::Wrap &&
                         config.font_size_explicitly_set &&
                         available_w > inset_x;
      if (wraps || config.label.find('\n') != std::string::npos) {
        const float max_w = wraps ? available_w - inset_x : 1e9f;
        const auto m = ui::measure_text_wrapped(config.label, max_w,
                                            config.font_name, font_px);
        return is_x ? m.width + inset_x : m.height;
      }
      const Vector2Type one =
          ui::measure_text_line(config.label, config.font_name, font_px);
      return is_x ? one.x + inset_x : one.y;
    }
    case Dim::Children:
    case Dim::Expand:
    case Dim::None:
      return 0.f;
    }
    return 0.f;
  };

  MeasuredConfig out;
  out.size = {axis_size(config.size.x_axis, available_w, screen_w, true),
              axis_size(config.size.y_axis, available_h, screen_h, false)};
  out.margin = {detail::measure_edge(config.margin.left, available_w, screen_w) +
                    detail::measure_edge(config.margin.right, available_w,
                                         screen_w),
                detail::measure_edge(config.margin.top, available_h, screen_h) +
                    detail::measure_edge(config.margin.bottom, available_h,
                                         screen_h)};
  return out;
}

} // namespace imm
} // namespace ui
} // namespace afterhours
