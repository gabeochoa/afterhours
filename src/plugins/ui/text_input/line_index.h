#pragma once

#include "../../../ecs.h"
#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

namespace afterhours {
namespace text_input {

/// Maps between byte offsets and row/column positions in text.
/// Caches line start positions for efficient lookups.
///
/// This is a UTILITY, not tied to any specific text storage.
/// Applications use it with their own buffer.
///
/// Usage:
/// ```cpp
/// LineIndex index;
/// index.rebuild(text);
///
/// auto pos = index.offset_to_position(cursor);
/// // pos.row, pos.column
///
/// size_t offset = index.position_to_offset(row, col);
/// ```
struct LineIndex {
  struct Position {
    size_t row;
    size_t column;
  };

  std::vector<size_t> line_starts_;
  size_t text_size_ = 0;

  /// Rebuild index from text. Call after text changes.
  void rebuild(std::string_view text) {
    line_starts_.clear();
    line_starts_.push_back(0); // Line 0 starts at offset 0

    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == '\n') {
        line_starts_.push_back(i + 1);
      }
    }
    text_size_ = text.size();
  }

  /// Number of lines (always >= 1)
  [[nodiscard]] size_t line_count() const {
    return line_starts_.empty() ? 1 : line_starts_.size();
  }

  /// Get byte offset of line start
  [[nodiscard]] size_t line_start(size_t row) const {
    if (row >= line_starts_.size())
      return text_size_;
    return line_starts_[row];
  }

  /// Get byte offset of line end (before newline or at text end)
  [[nodiscard]] size_t line_end(size_t row) const {
    if (row + 1 < line_starts_.size()) {
      return line_starts_[row + 1] - 1; // Before the \n
    }
    return text_size_;
  }

  /// Get line length in bytes (excluding newline)
  [[nodiscard]] size_t line_length(size_t row) const {
    return line_end(row) - line_start(row);
  }

  /// Convert byte offset to row/column
  [[nodiscard]] Position offset_to_position(size_t offset) const {
    if (line_starts_.empty()) {
      return {0, offset};
    }

    // Binary search for the line containing offset
    auto it =
        std::upper_bound(line_starts_.begin(), line_starts_.end(), offset);
    size_t row = (it == line_starts_.begin())
                     ? 0
                     : std::distance(line_starts_.begin(), it) - 1;
    size_t column = offset - line_starts_[row];
    return {row, column};
  }

  /// Convert row/column to byte offset
  [[nodiscard]] size_t position_to_offset(size_t row, size_t column) const {
    if (row >= line_starts_.size()) {
      return text_size_;
    }
    size_t start = line_starts_[row];
    size_t max_col = line_length(row);
    return start + std::min(column, max_col);
  }

  /// Clamp column to valid range for a row
  [[nodiscard]] size_t clamp_column(size_t row, size_t column) const {
    return std::min(column, line_length(row));
  }

  /// Get the text size this index was built for
  [[nodiscard]] size_t text_size() const { return text_size_; }
};

/// ECS component wrapper for LineIndex (pure data)
struct HasLineIndex : BaseComponent {
  LineIndex index;
  size_t last_text_hash = 0;

  HasLineIndex() = default;
};

} // namespace text_input
} // namespace afterhours
