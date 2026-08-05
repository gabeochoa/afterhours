#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "../../../ecs.h"
#include "../text_selection.h"

namespace afterhours {
namespace text_input {

/// A visual line after word wrapping.
/// Multiple VisualLines may correspond to one source line.
struct VisualLine {
  size_t source_offset = 0; // Byte offset in source text
  size_t length = 0;        // Bytes in this visual line
  float y_position = 0.f;   // Pixel Y from top of text area
  float width = 0.f;        // Pixel width of this line

  [[nodiscard]] size_t end_offset() const { return source_offset + length; }
};

/// Caches word-wrapped layout for efficient rendering.
/// Rebuild when text changes, wrap width changes, or font changes.
///
/// Wrapping itself is ui::detail::wrap_text_to_width -- the same primitive both
/// renderers use -- so an edited line breaks exactly where a drawn one does.
/// What this adds is the mapping the renderer does not need and an editor
/// cannot work without: which source byte each visual line starts at.
struct TextLayoutCache {
  std::vector<VisualLine> lines_;
  float total_height_ = 0.f;
  float max_width_ = 0.f;

  /// Rebuild layout from text.
  /// @param text The source text
  /// @param wrap_width Maximum line width in pixels (0 = no wrap)
  /// @param line_height Height of each line in pixels
  /// @param measure_fn Function to measure text width: (string_view) -> float
  template <typename MeasureFn>
  void rebuild(std::string_view text, float wrap_width, float line_height,
               MeasureFn measure_fn) {
    lines_.clear();
    total_height_ = 0.f;
    max_width_ = 0.f;

    const std::string source(text);
    // No-wrap still has to honour hard breaks, and a width nothing reaches is
    // how wrap_text_to_width expresses that (max_width <= 0 means "one line").
    const float width = wrap_width > 0.f ? wrap_width : 1e9f;
    const std::vector<std::string> wrapped = ui::detail::wrap_text_to_width(
        source, width,
        [&](const std::string &s) { return measure_fn(std::string_view(s)); });

    // Soft wrapping consumes the space it broke at, so a wrapped line's text is
    // not at a position arithmetic can predict -- but it IS the next verbatim
    // occurrence, because everything except break whitespace is preserved.
    size_t pos = 0;
    float y = 0.f;
    for (const std::string &line : wrapped) {
      const size_t at = source.find(line, pos);
      const size_t start = at == std::string::npos ? pos : at;
      const float w = line.empty() ? 0.f : measure_fn(std::string_view(line));
      lines_.push_back({start, line.size(), y, w});
      max_width_ = std::max(max_width_, w);
      y += line_height;
      pos = start + line.size();
    }

    // Ensure at least one line
    if (lines_.empty()) {
      lines_.push_back({0, 0, 0.f, 0.f});
      y = line_height;
    }

    total_height_ = y;
  }

  [[nodiscard]] const std::vector<VisualLine> &lines() const { return lines_; }
  [[nodiscard]] float total_height() const { return total_height_; }
  [[nodiscard]] float max_width() const { return max_width_; }
  [[nodiscard]] size_t line_count() const { return lines_.size(); }

  /// Find visual line index containing byte offset.
  /// The last line starting at or before `offset`: a cursor sitting at the end
  /// of a line belongs to that line, not to the next one and not to the last.
  [[nodiscard]] size_t line_at_offset(size_t offset) const {
    if (lines_.empty())
      return 0;
    size_t found = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
      if (lines_[i].source_offset > offset)
        break;
      found = i;
    }
    return found;
  }

  /// Byte offset of `offset` measured from the start of its visual line.
  [[nodiscard]] size_t column_at_offset(size_t offset) const {
    const VisualLine &l = line(line_at_offset(offset));
    if (offset <= l.source_offset)
      return 0;
    return std::min(offset - l.source_offset, l.length);
  }

  /// Find visual line index at pixel Y position
  [[nodiscard]] size_t line_at_y(float y, float line_height) const {
    if (y < 0 || lines_.empty() || line_height <= 0.f)
      return 0;
    size_t line = static_cast<size_t>(y / line_height);
    return std::min(line, lines_.size() - 1);
  }

  /// Get Y position for a byte offset
  [[nodiscard]] float y_for_offset(size_t offset) const {
    size_t line = line_at_offset(offset);
    return line < lines_.size() ? lines_[line].y_position : 0.f;
  }

  /// Get the visual line at an index
  [[nodiscard]] const VisualLine &line(size_t index) const {
    return lines_[std::min(index, lines_.size() - 1)];
  }

  /// The text of a visual line, sliced out of the source it was built from.
  [[nodiscard]] std::string line_text(std::string_view source,
                                      size_t index) const {
    const VisualLine &l = line(index);
    if (l.source_offset >= source.size())
      return "";
    return std::string(
        source.substr(l.source_offset,
                      std::min(l.length, source.size() - l.source_offset)));
  }
};

// Selection geometry over a wrapped block, in SOURCE byte offsets.
//
// ui::text_selection has equivalents, but they index into joined_text(), where
// a soft break becomes a '\n' the source never had -- fine for a read-only
// block, wrong for an editor whose cursor is a source offset. These take the
// same offsets the caret and the storage use.

/// Byte offset nearest a point local to the text block's top left.
template <typename MeasureFn>
inline size_t offset_at_point(const TextLayoutCache &cache,
                              std::string_view source, Vector2Type local,
                              float line_height, MeasureFn &&measure) {
  if (cache.line_count() == 0)
    return 0;
  const size_t row = cache.line_at_y(local.y, line_height);
  const std::string text = cache.line_text(source, row);
  // Walks UTF-8 characters, so a multi-byte glyph is never split.
  const size_t within = ui::text_selection::detail_sel::offset_nearest_x(
      text, local.x, std::forward<MeasureFn>(measure));
  return cache.line(row).source_offset + within;
}

/// One rect per visual row the range covers: a range inside one row yields one
/// rect, a range spanning three yields three with the first and last partial.
/// `origin` is the block's top left.
template <typename MeasureFn>
inline std::vector<RectangleType>
selection_rects(const TextLayoutCache &cache, std::string_view source,
                size_t start, size_t end, Vector2Type origin,
                float line_height, MeasureFn &&measure) {
  std::vector<RectangleType> rects;
  if (start >= end)
    return rects;

  for (size_t row = 0; row < cache.line_count(); row++) {
    const VisualLine &l = cache.line(row);
    // The break between rows is not drawn, so a range covering only it
    // contributes nothing on its own.
    if (l.end_offset() <= start || l.source_offset >= end)
      continue;

    const std::string text = cache.line_text(source, row);
    const size_t from = start > l.source_offset ? start - l.source_offset : 0;
    const size_t to =
        end < l.end_offset() ? end - l.source_offset : text.size();
    if (to <= from || from > text.size())
      continue;

    const float x0 = measure(std::string_view(text).substr(0, from));
    const float x1 = measure(std::string_view(text).substr(0, to));
    rects.push_back(RectangleType{origin.x + x0,
                                  origin.y + line_height *
                                                 static_cast<float>(row),
                                  x1 - x0, line_height});
  }
  return rects;
}

/// ECS component wrapper for TextLayoutCache
struct HasTextLayoutCache : BaseComponent {
  TextLayoutCache cache;
  uint64_t cached_version = 0; // For invalidation
  float cached_wrap_width = 0.f;
  float cached_line_height = 0.f;

  HasTextLayoutCache() = default;
};

} // namespace text_input
} // namespace afterhours
