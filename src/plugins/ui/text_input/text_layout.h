#pragma once

#include <algorithm>
#include <cctype>
#include <string_view>
#include <vector>

#include "../../../ecs.h"

namespace afterhours {
namespace text_input {

/// A visual line after word wrapping.
/// Multiple VisualLines may correspond to one source line.
struct VisualLine {
  size_t source_offset = 0; // Byte offset in source text
  size_t length = 0;        // Bytes in this visual line
  float y_position = 0.f;   // Pixel Y from top of text area
  float width = 0.f;        // Pixel width of this line
};

/// Caches word-wrapped layout for efficient rendering.
/// Rebuild when text changes, wrap width changes, or font changes.
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

    float y = 0.f;
    size_t pos = 0;

    while (pos <= text.size()) {
      // Find end of this source line
      size_t line_end = text.find('\n', pos);
      if (line_end == std::string_view::npos)
        line_end = text.size();

      std::string_view source_line = text.substr(pos, line_end - pos);

      if (wrap_width > 0 && !source_line.empty()) {
        wrap_line(source_line, pos, wrap_width, line_height, y, measure_fn);
      } else {
        float width = source_line.empty() ? 0.f : measure_fn(source_line);
        lines_.push_back({pos, source_line.size(), y, width});
        max_width_ = std::max(max_width_, width);
        y += line_height;
      }

      if (line_end >= text.size())
        break;
      pos = line_end + 1; // Skip past \n
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

  /// Find visual line index containing byte offset
  [[nodiscard]] size_t line_at_offset(size_t offset) const {
    for (size_t i = 0; i < lines_.size(); ++i) {
      if (offset >= lines_[i].source_offset &&
          offset < lines_[i].source_offset + lines_[i].length) {
        return i;
      }
    }
    return lines_.empty() ? 0 : lines_.size() - 1;
  }

  /// Find visual line index at pixel Y position
  [[nodiscard]] size_t line_at_y(float y, float line_height) const {
    if (y < 0 || lines_.empty())
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

private:
  template <typename MeasureFn>
  void wrap_line(std::string_view line, size_t base_offset, float wrap_width,
                 float line_height, float &y, MeasureFn measure_fn) {
    size_t pos = 0;

    while (pos < line.size()) {
      size_t end = pos;

      while (end < line.size()) {
        // Find next word boundary
        size_t word_end = end;
        while (word_end < line.size() && !std::isspace(line[word_end]))
          ++word_end;
        while (word_end < line.size() && std::isspace(line[word_end]))
          ++word_end;

        std::string_view segment = line.substr(pos, word_end - pos);
        if (measure_fn(segment) > wrap_width && end > pos) {
          break;
        }

        end = word_end;
      }

      // Force break if no break point found
      if (end == pos && pos < line.size()) {
        end = pos + 1;
        while (end < line.size() &&
               measure_fn(line.substr(pos, end - pos)) <= wrap_width) {
          ++end;
        }
        if (end > pos + 1)
          --end;
      }

      std::string_view visual_line = line.substr(pos, end - pos);
      float width = measure_fn(visual_line);
      lines_.push_back({base_offset + pos, end - pos, y, width});
      max_width_ = std::max(max_width_, width);
      y += line_height;
      pos = end;
    }

    // Handle empty line
    if (pos == 0) {
      lines_.push_back({base_offset, 0, y, 0.f});
      y += line_height;
    }
  }
};

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

