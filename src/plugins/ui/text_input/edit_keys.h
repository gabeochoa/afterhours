#pragma once

// Text editing keys for a field the app draws itself.
//
//   if (auto cmd = text_input::edit_command_from_keys())
//     text_input::apply_edit_command(my_field_state, *cmd);
//
// The imm text input reads these keys through the Text* names in the app's
// InputAction enum. A hand-drawn field has no widget to do that, so this reads
// this frame's keys against the same platform chords default_keymap binds
// (Option word ops and Cmd clipboard on macOS, Ctrl elsewhere) and returns the
// EditCommand they mean. Modifiers must match exactly, so Cmd+Shift+Z is Redo
// and never also Undo. Keys count on press and on key repeat.

#include <functional>
#include <optional>
#include <utility>

#include "../../ui.h"
#include "utils.h"

namespace afterhours {
namespace text_input {

inline std::optional<EditCommand> edit_command_from_keys() {
  using ui::DefaultAction;
  static const auto mapping = ui::default_keymap<DefaultAction>();
  static constexpr std::pair<DefaultAction, EditCommand> table[] = {
      {DefaultAction::TextBackspace, EditCommand::DeleteBack},
      {DefaultAction::TextDelete, EditCommand::DeleteForward},
      {DefaultAction::TextDeleteWordBack, EditCommand::DeleteWordBack},
      {DefaultAction::TextDeleteWordForward, EditCommand::DeleteWordForward},
      {DefaultAction::TextDeleteLineBack, EditCommand::DeleteLineBack},
      {DefaultAction::TextDeleteLineForward, EditCommand::DeleteLineForward},
      {DefaultAction::TextSelectAll, EditCommand::SelectAll},
      {DefaultAction::TextCopy, EditCommand::Copy},
      {DefaultAction::TextCut, EditCommand::Cut},
      {DefaultAction::TextPaste, EditCommand::Paste},
      {DefaultAction::TextUndo, EditCommand::Undo},
      {DefaultAction::TextRedo, EditCommand::Redo},
  };
  const uint8_t mods = input::get_current_modifiers();
  for (const auto &[action, cmd] : table) {
    auto it = mapping.find(static_cast<int>(action));
    if (it == mapping.end()) continue;
    for (const auto &any : it->second) {
      const auto *chord = std::get_if<input::KeyChord>(&any);
      if (!chord || chord->required_modifiers != mods) continue;
      if (input::is_key_pressed(chord->key) ||
          input::is_key_pressed_repeat(chord->key))
        return cmd;
    }
  }
  return std::nullopt;
}

struct LineEditKeys {
  std::function<bool(int)> is_pressed;
  std::function<bool(int)> is_down;
  std::function<int()> get_char;
};

inline LineEditKeys default_line_edit_keys() {
  LineEditKeys keys;
  keys.is_pressed = [](int key) {
    return input::is_key_pressed(key) || input::is_key_pressed_repeat(key);
  };
  keys.is_down = [](int key) { return input::is_key_down(key); };
  keys.get_char = []() { return input::get_char_pressed(); };
  return keys;
}

inline bool motion_from_keys(HasTextInputState &state,
                             const LineEditKeys &keys) {
  const size_t cursor_before = state.cursor_position;
  const std::optional<size_t> anchor_before = state.selection_anchor;

  const bool shift_held = keys.is_down(keys::LEFT_SHIFT) ||
                          keys.is_down(keys::RIGHT_SHIFT);
  const bool word_held =
      keys.is_down(keys::LEFT_CONTROL) || keys.is_down(keys::RIGHT_CONTROL) ||
      keys.is_down(keys::LEFT_ALT) || keys.is_down(keys::RIGHT_ALT);

  auto navigate = [&](auto move_fn) {
    if (shift_held && !state.selection_anchor)
      state.selection_anchor = state.cursor_position;
    move_fn();
    if (!shift_held)
      state.clear_selection();
  };

  if (keys.is_pressed(keys::LEFT)) {
    if (word_held) {
      navigate([&] {
        if (!shift_held && state.has_selection())
          state.cursor_position = state.selection_start();
        else
          move_cursor_word_left(state);
      });
    } else {
      navigate([&] {
        if (!shift_held && state.has_selection())
          state.cursor_position = state.selection_start();
        else
          move_cursor_left(state);
      });
    }
  }

  if (keys.is_pressed(keys::RIGHT)) {
    if (word_held) {
      navigate([&] {
        if (!shift_held && state.has_selection())
          state.cursor_position = state.selection_end();
        else
          move_cursor_word_right(state);
      });
    } else {
      navigate([&] {
        if (!shift_held && state.has_selection())
          state.cursor_position = state.selection_end();
        else
          move_cursor_right(state);
      });
    }
  }

  if (keys.is_pressed(keys::HOME))
    navigate([&] { state.cursor_position = 0; });

  if (keys.is_pressed(keys::END))
    navigate([&] { state.cursor_position = state.text_size(); });

  bool deleted = false;
  if (!state.readonly && keys.is_pressed(keys::BACKSPACE)) {
    state.push_undo_snapshot();
    if (state.has_selection())
      deleted = delete_selection(state);
    else if (word_held)
      deleted = delete_word_before_cursor(state);
    else
      deleted = delete_before_cursor(state);
  }

  if (!state.readonly && keys.is_pressed(keys::DELETE_KEY)) {
    state.push_undo_snapshot();
    if (state.has_selection())
      deleted = delete_selection(state) || deleted;
    else if (word_held)
      deleted = delete_word_after_cursor(state) || deleted;
    else
      deleted = delete_at_cursor(state) || deleted;
  }

  const bool changed = deleted || state.cursor_position != cursor_before ||
                       state.selection_anchor != anchor_before;
  if (changed)
    reset_blink(state);
  return changed;
}

inline bool motion_from_keys(HasTextInputState &state) {
  return motion_from_keys(state, default_line_edit_keys());
}

inline bool insert_typed_chars(HasTextInputState &state,
                               const LineEditKeys &keys) {
  if (state.readonly || !keys.get_char)
    return false;
  bool changed = false;
  for (int cp = keys.get_char(); cp > 0; cp = keys.get_char()) {
    if (cp < 32)
      continue;
    state.push_undo_snapshot();
    if (state.has_selection())
      delete_selection(state);
    if (insert_char(state, cp)) {
      state.clear_selection();
      changed = true;
    }
  }
  if (changed)
    reset_blink(state);
  return changed;
}

inline bool insert_typed_chars(HasTextInputState &state) {
  return insert_typed_chars(state, default_line_edit_keys());
}

inline bool edit_line_from_keys(HasTextInputState &state,
                                const LineEditKeys &keys) {
  bool changed = false;

  const bool shift_held = keys.is_down(keys::LEFT_SHIFT) ||
                          keys.is_down(keys::RIGHT_SHIFT);
#if defined(__APPLE__)
  const bool primary_held = keys.is_down(keys::LEFT_SUPER) ||
                            keys.is_down(keys::RIGHT_SUPER);
#else
  const bool primary_held = keys.is_down(keys::LEFT_CONTROL) ||
                            keys.is_down(keys::RIGHT_CONTROL);
#endif
  if (primary_held) {
    std::optional<EditCommand> cmd;
    if (shift_held && keys.is_pressed(keys::Z))
      cmd = EditCommand::Redo;
    else if (!shift_held && keys.is_pressed(keys::Z))
      cmd = EditCommand::Undo;
#if !defined(__APPLE__)
    else if (keys.is_pressed(keys::Y))
      cmd = EditCommand::Redo;
#endif
    else if (keys.is_pressed(keys::A))
      cmd = EditCommand::SelectAll;
    else if (keys.is_pressed(keys::C))
      cmd = EditCommand::Copy;
    else if (keys.is_pressed(keys::X))
      cmd = EditCommand::Cut;
    else if (keys.is_pressed(keys::V))
      cmd = EditCommand::Paste;
    if (cmd)
      changed = apply_edit_command(state, *cmd) || changed;
  }

  changed = motion_from_keys(state, keys) || changed;
  changed = insert_typed_chars(state, keys) || changed;
  return changed;
}

inline bool edit_line_from_keys(HasTextInputState &state) {
  return edit_line_from_keys(state, default_line_edit_keys());
}

} // namespace text_input
} // namespace afterhours
