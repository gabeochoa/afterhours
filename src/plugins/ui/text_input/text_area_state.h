#pragma once

#include "concepts.h"
#include "line_index.h"
#include "selection.h"
#include "state.h"
#include "text_layout.h"

namespace afterhours {
namespace text_input {

/// Configuration for text area behavior
struct TextAreaConfig {
  float line_height = 20.f; // Pixels per line
  bool word_wrap = true;    // Enable word wrapping
  size_t max_lines = 0;     // Maximum number of lines (0 = unlimited)
};

/// State component for multiline text input (text area).
/// Extends HasTextInputState with line navigation and layout caching.
template <TextStorage Storage = StringStorage>
struct HasTextAreaStateT : HasTextInputStateT<Storage> {
  LineIndex line_index;             // Row/column mapping
  TextLayoutCache layout_cache;     // Word wrap cache
  TextSelection selection;          // Selection (optional, for future)
  float scroll_offset_y = 0.f;      // Vertical scroll position
  size_t preferred_column = 0;      // For Up/Down preserving column
  uint64_t last_layout_version = 0; // For invalidation
  TextAreaConfig area_config;       // Text area specific config

  HasTextAreaStateT() = default;
  explicit HasTextAreaStateT(const std::string &initial_text,
                             size_t max_len = 0, // 0 = unlimited for text area
                             float blink_rate = 0.53f)
      : HasTextInputStateT<Storage>(initial_text, max_len, blink_rate) {
    rebuild_line_index();
  }

  /// Rebuild line index from current text
  void rebuild_line_index() {
    line_index.rebuild(this->text());
    last_layout_version++;
  }

  /// Check if layout needs rebuilding
  bool needs_layout_rebuild(uint64_t current_version) const {
    return last_layout_version != current_version;
  }

  /// Get current row/column from cursor position
  LineIndex::Position cursor_position_rc() const {
    return line_index.offset_to_position(this->cursor_position);
  }

  /// Get number of lines in the text
  size_t line_count() const { return line_index.line_count(); }

  /// Get visible line count (for viewport calculations)
  size_t visible_lines(float viewport_height) const {
    if (area_config.line_height <= 0)
      return 1;
    return static_cast<size_t>(viewport_height / area_config.line_height);
  }

  /// Ensure cursor is visible by adjusting scroll offset
  void ensure_cursor_visible(float viewport_height) {
    auto pos = cursor_position_rc();
    float cursor_y = static_cast<float>(pos.row) * area_config.line_height;

    // Scroll up if cursor is above viewport
    if (cursor_y < scroll_offset_y) {
      scroll_offset_y = cursor_y;
    }

    // Scroll down if cursor is below viewport
    float cursor_bottom = cursor_y + area_config.line_height;
    if (cursor_bottom > scroll_offset_y + viewport_height) {
      scroll_offset_y = cursor_bottom - viewport_height;
    }

    // Clamp scroll to valid range
    float max_scroll = std::max(0.f, static_cast<float>(line_count()) *
                                             area_config.line_height -
                                         viewport_height);
    scroll_offset_y = std::max(0.f, std::min(scroll_offset_y, max_scroll));
  }
};

/// Default alias for simple std::string-based text area
using HasTextAreaState = HasTextAreaStateT<StringStorage>;

} // namespace text_input
} // namespace afterhours
