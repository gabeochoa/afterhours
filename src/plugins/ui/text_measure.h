#pragma once

// Measuring and wrapping text, for apps.
//
// The ui::detail helpers want a `measure` callable the caller builds out of
// FontManager; these take a font name and size and do that lookup themselves.
// Same TextMeasureCache -> FontManager path as the layout pass
// (AutoLayout::get_text_size_for_axis), so the two answers cannot drift.

#include <string>
#include <vector>

#include "../../core/text_cache.h"
#include "../../ecs.h"
#include "text_selection.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

/// Size of a single line, no wrapping. Cache first, FontManager second.
inline Vector2Type measure_text_line(const std::string &text,
                                     const std::string &font_name,
                                     float font_size, float spacing = 1.f) {
  if (auto *cache = EntityHelper::get_singleton_cmp<TextMeasureCache>())
    return cache->measure(text, font_name, font_size, spacing);
  auto *fonts = EntityHelper::get_singleton_cmp<FontManager>();
  if (!fonts)
    return {0.f, 0.f};
  return measure_text(fonts->get_font(font_name), text.c_str(), font_size,
                      spacing);
}

/// Lines `text` would occupy at `max_width`. Honors hard '\n'. A word wider
/// than max_width gets its own line rather than being split.
inline std::vector<std::string> wrap_text(const std::string &text,
                                          float max_width,
                                          const std::string &font_name,
                                          float font_size,
                                          float spacing = 1.f) {
  return detail::wrap_text_to_width(
      text, max_width, [&](const std::string &s) {
        return measure_text_line(s, font_name, font_size, spacing).x;
      });
}

/// Width, height and line count of `text` laid out at `max_width` -- the
/// answer to "how tall is this paragraph".
inline detail::WrappedTextMetrics
measure_text_wrapped(const std::string &text, float max_width,
                     const std::string &font_name, float font_size,
                     float spacing = 1.f) {
  return detail::measure_wrapped(
      text, max_width, [&](const std::string &s) {
        return measure_text_line(s, font_name, font_size, spacing);
      });
}

} // namespace ui
} // namespace afterhours
