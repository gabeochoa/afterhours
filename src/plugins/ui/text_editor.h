// Text Editor Component for Afterhours
// Gap 03: Text Editing Widget
//
// Provides a multiline text editor with:
// - Selection (shift+arrow, click-drag, double-click word, triple-click line)
// - Undo/redo integration (via CommandHistory)
// - Clipboard support (via pluggable clipboard)
// - Word wrap mode
// - Read-only mode
// - Per-range styling hooks

#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "../../ecs.h"
#include "../clipboard.h"
#include "../command_history.h"
#include "scroll_view.h"

namespace afterhours {
namespace ui {

//=============================================================================
// TEXT EDITOR STATE
//=============================================================================

/// Position in text buffer (line, column)
struct TextPosition {
  size_t line = 0;
  size_t column = 0;

  bool operator==(const TextPosition& other) const {
    return line == other.line && column == other.column;
  }

  bool operator!=(const TextPosition& other) const { return !(*this == other); }

  bool operator<(const TextPosition& other) const {
    return line < other.line || (line == other.line && column < other.column);
  }

  bool operator<=(const TextPosition& other) const {
    return *this < other || *this == other;
  }

  bool operator>(const TextPosition& other) const { return !(*this <= other); }

  bool operator>=(const TextPosition& other) const { return !(*this < other); }
};

/// Text selection range
struct TextSelection {
  TextPosition start;
  TextPosition end;

  bool is_empty() const { return start == end; }

  bool is_valid() const { return start <= end; }

  void normalize() {
    if (start > end) std::swap(start, end);
  }

  TextPosition min() const { return start < end ? start : end; }
  TextPosition max() const { return start < end ? end : start; }
};

/// Style applied to a range of text
struct TextStyleRange {
  size_t start_offset = 0;  // Absolute character offset
  size_t end_offset = 0;
  Color color{0, 0, 0, 255};
  Color background{0, 0, 0, 0};  // Transparent = no background
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool strikethrough = false;
};

/// Configuration for text editor behavior
struct TextEditorConfig {
  bool word_wrap = true;
  bool read_only = false;
  bool show_line_numbers = false;
  bool highlight_current_line = true;
  bool auto_indent = true;
  float tab_size = 4.f;          // Spaces per tab
  float line_height = 1.2f;      // Multiplier
  float font_size = 14.f;
  float cursor_blink_rate = 0.5f;  // Seconds per blink cycle
  Color cursor_color{0, 0, 0, 255};
  Color selection_color{51, 153, 255, 100};
  Color current_line_color{255, 255, 200, 50};
  Color line_number_color{128, 128, 128, 255};
};

/// Text editor state component
template <typename Storage = std::vector<std::string>>
struct TextEditorState : BaseComponent {
  // Content storage (lines)
  Storage lines;

  // Cursor position
  TextPosition cursor;

  // Selection anchor (for shift+arrow, click-drag)
  TextPosition selection_anchor;

  // Is there an active selection?
  bool has_selection_flag = false;

  // Configuration
  TextEditorConfig config;

  // Scroll integration
  float scroll_x = 0.f;
  float scroll_y = 0.f;

  // Cursor blink state
  float cursor_blink_timer = 0.f;
  bool cursor_visible = true;

  // Undo/redo history
  CommandHistory<std::string> history;

  // Style ranges (optional, for syntax highlighting etc.)
  std::vector<TextStyleRange> style_ranges;

  // Callback for when text changes
  std::function<void(const std::string&)> on_change;

  // === Initialization ===

  TextEditorState() { lines.push_back(""); }

  TextEditorState(const std::string& initial_text) { set_text(initial_text); }

  // === Text Access ===

  std::string get_text() const {
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
      if (i > 0) result += '\n';
      result += lines[i];
    }
    return result;
  }

  void set_text(const std::string& text) {
    lines.clear();
    std::string current_line;
    for (char c : text) {
      if (c == '\n') {
        lines.push_back(current_line);
        current_line.clear();
      } else {
        current_line += c;
      }
    }
    lines.push_back(current_line);
    cursor = {0, 0};
    selection_anchor = cursor;
    has_selection_flag = false;
    if (on_change) on_change(get_text());
  }

  size_t line_count() const { return lines.size(); }

  const std::string& get_line(size_t index) const {
    static const std::string empty;
    return index < lines.size() ? lines[index] : empty;
  }

  size_t line_length(size_t index) const {
    return index < lines.size() ? lines[index].size() : 0;
  }

  // === Selection ===

  bool has_selection() const { return has_selection_flag && cursor != selection_anchor; }

  TextSelection get_selection() const {
    TextSelection sel;
    if (cursor < selection_anchor) {
      sel.start = cursor;
      sel.end = selection_anchor;
    } else {
      sel.start = selection_anchor;
      sel.end = cursor;
    }
    return sel;
  }

  std::string get_selected_text() const {
    if (!has_selection()) return "";

    auto sel = get_selection();
    std::string result;

    for (size_t line = sel.start.line; line <= sel.end.line && line < lines.size();
         ++line) {
      size_t start_col = (line == sel.start.line) ? sel.start.column : 0;
      size_t end_col =
          (line == sel.end.line) ? sel.end.column : lines[line].size();
      if (start_col < lines[line].size()) {
        result += lines[line].substr(start_col, end_col - start_col);
      }
      if (line < sel.end.line) result += '\n';
    }

    return result;
  }

  void clear_selection() {
    has_selection_flag = false;
    selection_anchor = cursor;
  }

  void select_all() {
    selection_anchor = {0, 0};
    cursor = {lines.size() - 1, lines.empty() ? 0 : lines.back().size()};
    has_selection_flag = true;
  }

  void start_selection() {
    selection_anchor = cursor;
    has_selection_flag = true;
  }

  // === Cursor Movement ===

  void move_cursor_left(bool extend_selection = false) {
    if (!extend_selection && has_selection()) {
      cursor = get_selection().start;
      clear_selection();
      return;
    }
    if (extend_selection && !has_selection_flag) start_selection();

    if (cursor.column > 0) {
      cursor.column--;
    } else if (cursor.line > 0) {
      cursor.line--;
      cursor.column = lines[cursor.line].size();
    }
  }

  void move_cursor_right(bool extend_selection = false) {
    if (!extend_selection && has_selection()) {
      cursor = get_selection().end;
      clear_selection();
      return;
    }
    if (extend_selection && !has_selection_flag) start_selection();

    if (cursor.column < line_length(cursor.line)) {
      cursor.column++;
    } else if (cursor.line < lines.size() - 1) {
      cursor.line++;
      cursor.column = 0;
    }
  }

  void move_cursor_up(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();

    if (cursor.line > 0) {
      cursor.line--;
      cursor.column = std::min(cursor.column, line_length(cursor.line));
    }
  }

  void move_cursor_down(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();

    if (cursor.line < lines.size() - 1) {
      cursor.line++;
      cursor.column = std::min(cursor.column, line_length(cursor.line));
    }
  }

  void move_cursor_to_line_start(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();
    cursor.column = 0;
  }

  void move_cursor_to_line_end(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();
    cursor.column = line_length(cursor.line);
  }

  void move_cursor_word_left(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();

    if (cursor.column == 0 && cursor.line > 0) {
      cursor.line--;
      cursor.column = line_length(cursor.line);
      return;
    }

    const std::string& line = lines[cursor.line];
    size_t pos = cursor.column;

    // Skip whitespace
    while (pos > 0 && std::isspace(line[pos - 1])) pos--;
    // Skip word
    while (pos > 0 && !std::isspace(line[pos - 1])) pos--;

    cursor.column = pos;
  }

  void move_cursor_word_right(bool extend_selection = false) {
    if (extend_selection && !has_selection_flag) start_selection();
    if (!extend_selection) clear_selection();

    const std::string& line = lines[cursor.line];

    if (cursor.column >= line.size() && cursor.line < lines.size() - 1) {
      cursor.line++;
      cursor.column = 0;
      return;
    }

    size_t pos = cursor.column;
    // Skip word
    while (pos < line.size() && !std::isspace(line[pos])) pos++;
    // Skip whitespace
    while (pos < line.size() && std::isspace(line[pos])) pos++;

    cursor.column = pos;
  }

  // === Text Editing ===

  void insert_char(char c) {
    if (config.read_only) return;
    delete_selection();

    if (c == '\n') {
      // Split line
      std::string remainder = lines[cursor.line].substr(cursor.column);
      lines[cursor.line] = lines[cursor.line].substr(0, cursor.column);
      lines.insert(lines.begin() + static_cast<long>(cursor.line + 1), remainder);
      cursor.line++;
      cursor.column = 0;
    } else {
      lines[cursor.line].insert(cursor.column, 1, c);
      cursor.column++;
    }

    clear_selection();
    if (on_change) on_change(get_text());
  }

  void insert_text(const std::string& text) {
    if (config.read_only) return;
    delete_selection();

    for (char c : text) {
      insert_char(c);
    }
  }

  void delete_char_before() {
    if (config.read_only) return;

    if (has_selection()) {
      delete_selection();
      return;
    }

    if (cursor.column > 0) {
      lines[cursor.line].erase(cursor.column - 1, 1);
      cursor.column--;
    } else if (cursor.line > 0) {
      // Join with previous line
      cursor.column = lines[cursor.line - 1].size();
      lines[cursor.line - 1] += lines[cursor.line];
      lines.erase(lines.begin() + static_cast<long>(cursor.line));
      cursor.line--;
    }

    if (on_change) on_change(get_text());
  }

  void delete_char_after() {
    if (config.read_only) return;

    if (has_selection()) {
      delete_selection();
      return;
    }

    if (cursor.column < lines[cursor.line].size()) {
      lines[cursor.line].erase(cursor.column, 1);
    } else if (cursor.line < lines.size() - 1) {
      // Join with next line
      lines[cursor.line] += lines[cursor.line + 1];
      lines.erase(lines.begin() + static_cast<long>(cursor.line + 1));
    }

    if (on_change) on_change(get_text());
  }

  void delete_selection() {
    if (!has_selection() || config.read_only) return;

    auto sel = get_selection();

    if (sel.start.line == sel.end.line) {
      // Single line delete
      lines[sel.start.line].erase(sel.start.column,
                                   sel.end.column - sel.start.column);
    } else {
      // Multi-line delete
      std::string new_line =
          lines[sel.start.line].substr(0, sel.start.column) +
          lines[sel.end.line].substr(sel.end.column);
      lines[sel.start.line] = new_line;
      lines.erase(lines.begin() + static_cast<long>(sel.start.line + 1),
                  lines.begin() + static_cast<long>(sel.end.line + 1));
    }

    cursor = sel.start;
    clear_selection();
    if (on_change) on_change(get_text());
  }

  // === Clipboard ===

  void cut() {
    if (!has_selection() || config.read_only) return;
    clipboard::set_text(get_selected_text());
    delete_selection();
  }

  void copy() {
    if (!has_selection()) return;
    clipboard::set_text(get_selected_text());
  }

  void paste() {
    if (config.read_only) return;
    std::string text = clipboard::get_text();
    if (!text.empty()) {
      insert_text(text);
    }
  }

  // === Undo/Redo ===

  void save_undo_state() {
    history.push(std::make_unique<SimpleCommand<std::string>>(
        get_text(),
        [this](const std::string& state) { set_text(state); },
        [this](const std::string& state) { set_text(state); }));
  }

  void undo() {
    if (history.can_undo()) {
      history.undo();
    }
  }

  void redo() {
    if (history.can_redo()) {
      history.redo();
    }
  }

  // === Scroll Integration ===

  void scroll_to_cursor(HasScrollView& scroll_view, float char_width,
                        float line_height) {
    float cursor_x = static_cast<float>(cursor.column) * char_width;
    float cursor_y = static_cast<float>(cursor.line) * line_height;
    scroll_view.scroll_to_visible(cursor_x, cursor_y, char_width, line_height,
                                  line_height);
  }

  // === Cursor Blink ===

  void update_cursor_blink(float dt) {
    cursor_blink_timer += dt;
    if (cursor_blink_timer >= config.cursor_blink_rate) {
      cursor_blink_timer = 0.f;
      cursor_visible = !cursor_visible;
    }
  }

  void reset_cursor_blink() {
    cursor_blink_timer = 0.f;
    cursor_visible = true;
  }
};

//=============================================================================
// IMMEDIATE MODE TEXT EDITOR
//=============================================================================

/// Result of text_editor() call
struct TextEditorResult {
  bool changed = false;      // Text was modified
  bool focused = false;      // Editor has focus
  bool lost_focus = false;   // Focus was just lost
};

/// Create an immediate-mode text editor
template <typename Storage = std::vector<std::string>>
inline TextEditorResult text_editor(const std::string& id,
                                    TextEditorState<Storage>& state,
                                    float width, float height) {
  TextEditorResult result;
  // Implementation would integrate with the immediate-mode UI system
  // For now, this is a stub - actual rendering happens in the render system
  return result;
}

//=============================================================================
// SIMPLE COMMAND FOR UNDO
//=============================================================================

template <typename T>
struct SimpleCommand : Command {
  T saved_state;
  std::function<void(const T&)> apply_fn;
  std::function<void(const T&)> revert_fn;

  SimpleCommand(T state, std::function<void(const T&)> apply,
                std::function<void(const T&)> revert)
      : saved_state(std::move(state)),
        apply_fn(std::move(apply)),
        revert_fn(std::move(revert)) {}

  void execute() override { apply_fn(saved_state); }
  void undo() override { revert_fn(saved_state); }
  std::string description() const override { return "Text edit"; }
};

}  // namespace ui
}  // namespace afterhours
