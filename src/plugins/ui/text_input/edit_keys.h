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

} // namespace text_input
} // namespace afterhours
