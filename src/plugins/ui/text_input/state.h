#pragma once

#include "../../../ecs.h"
#include "concepts.h"
#include "storage.h"
#include <functional>
#include <string>
#include <vector>

namespace afterhours {
namespace text_input {

// Text input state - templated on storage backend
// Use HasTextInputState for default std::string storage
// Use HasTextInputStateT<YourStorage> for custom backends (gap buffer, rope)
template <TextStorage Storage = StringStorage>
struct HasTextInputStateT : BaseComponent {
  Storage storage;
  size_t cursor_position = 0; // Byte position in UTF-8 string
  bool changed_since = false;
  bool is_focused = false;
  bool was_focused = false; // Previous frame's focus state (for detecting transitions)
  bool readonly = false;  // Focus/select OK, mutations blocked
  bool disabled = false;  // Focus blocked, no interaction at all
  size_t max_length = 256; // Maximum text length in bytes (0 = unlimited)
  float cursor_blink_timer = 0.f;  // Current timer value
  float cursor_blink_rate = 0.53f; // Seconds per half-cycle (configurable)

  // Selection: when set, text between selection_anchor and cursor_position
  // is selected. The anchor is the fixed end; cursor moves the other end.
  std::optional<size_t> selection_anchor;

  // Multi-click tracking for double/triple click word/line selection
  float last_click_time = -1.f;
  size_t last_click_pos = 0;
  int click_count = 0;

  // Horizontal scroll offset for text that exceeds field width
  float scroll_offset_x = 0.f;

  // Undo/redo snapshot stack
  struct Snapshot {
    std::string text;
    size_t cursor;
  };
  std::vector<Snapshot> undo_stack;
  size_t undo_index = 0;
  static constexpr size_t MAX_UNDO = 50;

  void push_undo_snapshot() {
    std::string t = text();
    if (!undo_stack.empty() && undo_index > 0 &&
        undo_stack[undo_index - 1].text == t)
      return;
    if (undo_index < undo_stack.size())
      undo_stack.resize(undo_index);
    undo_stack.push_back({t, cursor_position});
    if (undo_stack.size() > MAX_UNDO)
      undo_stack.erase(undo_stack.begin());
    undo_index = undo_stack.size();
  }

  bool undo() {
    if (undo_index == 0) return false;
    if (undo_index == undo_stack.size()) {
      push_undo_snapshot();
      undo_index--;
    }
    undo_index--;
    auto &snap = undo_stack[undo_index];
    storage.clear();
    storage.insert(0, snap.text);
    cursor_position = std::min(snap.cursor, storage.size());
    clear_selection();
    changed_since = true;
    return true;
  }

  bool redo() {
    if (undo_index + 1 >= undo_stack.size()) return false;
    undo_index++;
    auto &snap = undo_stack[undo_index];
    storage.clear();
    storage.insert(0, snap.text);
    cursor_position = std::min(snap.cursor, storage.size());
    clear_selection();
    changed_since = true;
    return true;
  }

  HasTextInputStateT() = default;
  explicit HasTextInputStateT(const std::string &initial_text,
                              size_t max_len = 256, float blink_rate = 0.53f)
      : storage(initial_text), cursor_position(initial_text.size()),
        max_length(max_len), cursor_blink_rate(blink_rate) {}

  // Convenience accessors
  std::string text() const { return storage.str(); }
  size_t text_size() const { return storage.size(); }

  bool has_selection() const {
    return selection_anchor.has_value() &&
           *selection_anchor != cursor_position;
  }
  size_t selection_start() const {
    if (!selection_anchor) return cursor_position;
    return std::min(*selection_anchor, cursor_position);
  }
  size_t selection_end() const {
    if (!selection_anchor) return cursor_position;
    return std::max(*selection_anchor, cursor_position);
  }
  std::string selected_text() const {
    if (!has_selection()) return "";
    return storage.str().substr(selection_start(),
                                selection_end() - selection_start());
  }
  void clear_selection() { selection_anchor.reset(); }
};

// Default alias for simple std::string-based text input
using HasTextInputState = HasTextInputStateT<StringStorage>;

// Listener for text input events (character typing)
struct HasTextInputListener : BaseComponent {
  std::function<void(Entity &, const std::string &)> on_change;
  std::function<void(Entity &)> on_submit; // Called on Enter key

  HasTextInputListener(
      std::function<void(Entity &, const std::string &)> change_cb = nullptr,
      std::function<void(Entity &)> submit_cb = nullptr)
      : on_change(std::move(change_cb)), on_submit(std::move(submit_cb)) {}
};

} // namespace text_input
} // namespace afterhours
