#pragma once

// Text editing for a field the app draws itself (Pumpkin G/L).
//
//   text_input::RawTextEdit field{"127.0.0.1"};
//   text_input::update_raw_text_edit(field, dt);  // every frame
//   // draw field.text() with the caret at field.cursor_position yourself
//
// The state is the widget's own state struct, minus the widget: every edit
// runs through the same ops in utils.h (insert_char, the word/line deletes,
// insert_pasted_text) and the same platform chords as edit_keys.h, so a
// hand-drawn field and an imm text_input can never drift apart.

#include "edit_keys.h"
#include "state.h"

namespace afterhours {
namespace text_input {

// Not a second state: the raw-draw field owns this directly instead of
// hanging it off an entity.
using RawTextEdit = HasTextInputState;

inline bool raw_text_cursor_visible(const RawTextEdit &s) {
  return s.cursor_blink_timer < s.cursor_blink_rate;
}

/// One frame of editing: blink, edit/clipboard chords, the char queue, then
/// caret movement (Shift extends the selection, the platform word modifier
/// moves by word). Returns true when the text changed.
inline bool update_raw_text_edit(RawTextEdit &s, float dt) {
  update_blink(s, dt);
  bool changed = false;

  if (auto cmd = edit_command_from_keys())
    changed = apply_edit_command(s, *cmd) || changed;

  if (s.readonly) {
    while (input::get_char_pressed() > 0) {
    }
  } else {
    for (int cp = input::get_char_pressed(); cp > 0;
         cp = input::get_char_pressed()) {
      s.push_undo_snapshot();
      if (s.has_selection()) delete_selection(s);
      if (insert_char(s, cp)) {
        s.clear_selection();
        reset_blink(s);
        changed = true;
      }
    }
  }

  const bool shift = input::is_key_down(keys::LEFT_SHIFT) ||
                     input::is_key_down(keys::RIGHT_SHIFT);
#if defined(__APPLE__)
  const bool word_mod = input::is_key_down(keys::LEFT_ALT) ||
                        input::is_key_down(keys::RIGHT_ALT);
#else
  const bool word_mod = input::is_key_down(keys::LEFT_CONTROL) ||
                        input::is_key_down(keys::RIGHT_CONTROL);
#endif
  auto pressed_repeat = [](int key) {
    return input::is_key_pressed(key) || input::is_key_pressed_repeat(key);
  };
  // Same collapse/extend rule as the widget's navigate().
  auto navigate = [&](auto move) {
    if (shift) {
      if (!s.selection_anchor) s.selection_anchor = s.cursor_position;
    }
    move();
    if (!shift) s.clear_selection();
    reset_blink(s);
  };
  if (pressed_repeat(keys::LEFT)) {
    navigate([&] {
      if (!shift && s.has_selection())
        s.cursor_position = s.selection_start();
      else if (word_mod)
        move_cursor_word_left(s);
      else
        move_cursor_left(s);
    });
  }
  if (pressed_repeat(keys::RIGHT)) {
    navigate([&] {
      if (!shift && s.has_selection())
        s.cursor_position = s.selection_end();
      else if (word_mod)
        move_cursor_word_right(s);
      else
        move_cursor_right(s);
    });
  }
  if (pressed_repeat(keys::HOME))
    navigate([&] { s.cursor_position = 0; });
  if (pressed_repeat(keys::END))
    navigate([&] { s.cursor_position = s.text_size(); });

  return changed;
}

} // namespace text_input
} // namespace afterhours
