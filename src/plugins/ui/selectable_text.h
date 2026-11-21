#pragma once

// Opt-in via with_selectable_text(). Offsets are source bytes via text_input's
// wrap mapping, so soft wraps invent no offsets. Cross-element/virtualized: out of scope.

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../../ecs.h"
#include "text_input/text_layout.h"
#include "text_selection.h"
#include "ui_core_components.h"

namespace afterhours {
namespace ui {

struct HasSelectableText : BaseComponent {
  // Shared layout: hit-test and renderer must agree on line breaks.
  text_input::TextLayoutCache layout;
  std::string cached_text;
  float cached_wrap_width = -1.f;
  float cached_font_size = -1.f;
  float cached_line_height = -1.f;
  text_selection::ClickStreak clicks;
};

namespace selectable_text {

inline float line_shift(TextAlignment alignment, float avail_width,
                        float line_width) {
  switch (alignment) {
  case TextAlignment::Center:
    return std::max(0.f, (avail_width - line_width) * 0.5f);
  case TextAlignment::Right:
    return std::max(0.f, avail_width - line_width);
  case TextAlignment::Left:
  case TextAlignment::None:
    break;
  }
  return 0.f;
}

template <typename MeasureFn>
inline bool refresh_layout(HasSelectableText &sel, const std::string &text,
                           float wrap_width, float font_size,
                           float line_height, MeasureFn &&measure) {
  if (sel.cached_text == text && sel.cached_wrap_width == wrap_width &&
      sel.cached_font_size == font_size &&
      sel.cached_line_height == line_height)
    return false;
  sel.layout.rebuild(text, wrap_width, line_height,
                     std::forward<MeasureFn>(measure));
  sel.cached_text = text;
  sel.cached_wrap_width = wrap_width;
  sel.cached_font_size = font_size;
  sel.cached_line_height = line_height;
  return true;
}

// `local` is block top-left; rows clamp like a drag off the edge.
template <typename MeasureFn>
inline size_t offset_at(const text_input::TextLayoutCache &cache,
                        std::string_view source, Vector2Type local,
                        float line_height, TextAlignment alignment,
                        float avail_width, MeasureFn &&measure) {
  if (cache.line_count() == 0 || line_height <= 0.f)
    return 0;
  const size_t row = cache.line_at_y(local.y, line_height);
  const text_input::VisualLine &line = cache.line(row);
  const std::string text = cache.line_text(source, row);
  const float shift = line_shift(alignment, avail_width, line.width);
  const size_t within = text_selection::detail_sel::offset_nearest_x(
      text, local.x - shift, std::forward<MeasureFn>(measure));
  return line.source_offset + within;
}

// Row break not drawn; a range covering only it paints nothing.
template <typename MeasureFn>
inline std::vector<RectangleType>
selection_rects(const text_input::TextLayoutCache &cache,
                std::string_view source, text_selection::Range range,
                Vector2Type origin, float line_height,
                TextAlignment alignment, float avail_width,
                MeasureFn &&measure) {
  std::vector<RectangleType> rects;
  if (range.empty())
    return rects;
  for (size_t row = 0; row < cache.line_count(); row++) {
    const text_input::VisualLine &line = cache.line(row);
    if (line.end_offset() <= range.start || line.source_offset >= range.end)
      continue;
    const std::string text = cache.line_text(source, row);
    const size_t from =
        range.start > line.source_offset ? range.start - line.source_offset : 0;
    const size_t to =
        range.end < line.end_offset() ? range.end - line.source_offset
                                      : text.size();
    if (to <= from || from > text.size())
      continue;
    const float x0 = measure(text.substr(0, from));
    const float x1 = measure(text.substr(0, to));
    const float shift = line_shift(alignment, avail_width, line.width);
    rects.push_back(RectangleType{origin.x + shift + x0,
                                  origin.y + line_height * static_cast<float>(row),
                                  x1 - x0, line_height});
  }
  return rects;
}

inline void apply_click(text_selection::Selection &sel, const std::string &text,
                        size_t pos, int streak, bool extend) {
  if (streak >= 3) {
    const text_selection::Range r = text_selection::line_range_at(text, pos);
    sel.anchor = r.start;
    sel.cursor = r.end;
  } else if (streak == 2) {
    const text_selection::Range r = text_selection::word_range_at(text, pos);
    sel.anchor = r.start;
    sel.cursor = r.end;
  } else if (extend) {
    sel.cursor = pos;
  } else {
    sel.collapse_to(pos);
  }
}

} // namespace selectable_text
} // namespace ui
} // namespace afterhours
