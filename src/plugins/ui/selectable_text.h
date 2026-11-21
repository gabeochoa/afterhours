#pragma once

// Selectable read-only text for labels (decide-selectable-text, option A).
//
// A label opted in with ComponentConfig::with_selectable_text() can be
// dragged over, shift-clicked, double-clicked (word) and triple-clicked
// (line), and copied with TextCopy. The one active selection lives on
// UIContext (entity + anchor/cursor byte offsets into the label's SOURCE
// string), so a press anywhere else clears it and the element itself stays
// out of the tab order -- it is text, not a widget.
//
// Offsets are source bytes throughout: the layout is text_input's
// TextLayoutCache over detail::wrap_lines, the same mapping the editor
// uses, so a soft wrap never invents offsets the source does not have.
// Cross-element selection and virtualized sources are deliberately out of
// scope (that is the TextSource controller, option C of the decision note).

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
  // Wrapped layout of the label's current text, rebuilt when the text, the
  // wrap width, the font size or the line height changes. Shared by the
  // input system (points -> offsets) and the renderer (selection bands), so
  // the two can never disagree about where a line breaks.
  text_input::TextLayoutCache layout;
  std::string cached_text;
  float cached_wrap_width = -1.f;
  float cached_font_size = -1.f;
  float cached_line_height = -1.f;
  text_selection::ClickStreak clicks;
};

namespace selectable_text {

// How far a line's glyphs sit from the block's left edge under its
// alignment. Left-aligned text (the only kind text_input draws) shifts by
// nothing.
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

// Rebuild the cached layout when anything it depends on moved. `measure`
// sizes a string in pixels at the label's font, like everywhere else here.
// Returns true when it rebuilt.
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

// Which source byte a point lands on. `local` is relative to the text
// block's top left (the first line's band origin); rows clamp to the block,
// like a drag that runs off the top or bottom.
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

// One band per visual row the range covers, at the same origin offset_at
// measures points from. The break between rows is not drawn, so a range
// covering only it paints nothing.
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

// What one press does to a selection: a plain press collapses to the press
// point, shift extends the cursor, a second press takes the word, a third
// takes the hard line.
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
