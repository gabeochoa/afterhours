#pragma once

#include "../../../ecs.h"
#include <algorithm>
#include <cstddef>

namespace afterhours {
namespace text_input {

/// Text selection state - tracks anchor and cursor positions.
/// The anchor is where selection started, cursor is current position.
/// When anchor == cursor, there is no selection (just a caret).
///
/// This is just DATA - no editing logic. Applications use this
/// with their own text buffer to implement selection behavior.
struct TextSelection {
  size_t anchor = 0; // Byte offset where selection started
  size_t cursor = 0; // Byte offset of current cursor position

  // Query
  [[nodiscard]] bool has_selection() const { return anchor != cursor; }
  [[nodiscard]] bool is_empty() const { return !has_selection(); }

  // Get ordered range (start <= end)
  [[nodiscard]] size_t start() const { return std::min(anchor, cursor); }
  [[nodiscard]] size_t end() const { return std::max(anchor, cursor); }
  [[nodiscard]] size_t length() const { return end() - start(); }

  // Mutations
  void collapse_to_cursor() { anchor = cursor; }
  void collapse_to_start() { cursor = anchor = start(); }
  void collapse_to_end() { cursor = anchor = end(); }
  void select_all(size_t text_length) {
    anchor = 0;
    cursor = text_length;
  }

  // Set cursor, optionally extending selection
  void set_cursor(size_t pos, bool extend_selection) {
    cursor = pos;
    if (!extend_selection) {
      anchor = pos;
    }
  }

  // Move cursor by offset, optionally extending selection
  void move_cursor(size_t new_pos, bool extend_selection) {
    set_cursor(new_pos, extend_selection);
  }
};

struct HasTextSelection : BaseComponent {
  TextSelection selection;

  HasTextSelection() = default;
  explicit HasTextSelection(size_t initial_cursor)
      : selection{initial_cursor, initial_cursor} {}
};

} // namespace text_input
} // namespace afterhours
