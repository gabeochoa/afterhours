// edit_keys_test.cpp
// text_input::edit_command_from_keys: a hand-drawn field gets the same
// platform chords as the imm text input. Chords come from default_keymap, so
// this holds on macOS and elsewhere; modifiers must match exactly.

// The app has no imm text input, so the missing-Text*-names warning is off.
#define AFTERHOURS_NO_IMM_TEXT_INPUT

#include <cstdio>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/ui/text_input/edit_keys.h>

using namespace afterhours;
using text_input::EditCommand;
using ui::DefaultAction;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static input::KeyChord chord_for(DefaultAction action) {
  static const auto mapping = ui::default_keymap<DefaultAction>();
  return std::get<input::KeyChord>(mapping.at(static_cast<int>(action)).front());
}

static void hold_mods(uint8_t mods, bool down) {
  using KC = input::KeyChord;
  const std::pair<uint8_t, int> mod_keys[] = {
      {KC::MOD_SHIFT, keys::LEFT_SHIFT},
      {KC::MOD_CTRL, keys::LEFT_CONTROL},
      {KC::MOD_ALT, keys::LEFT_ALT},
      {KC::MOD_SUPER, keys::LEFT_SUPER},
  };
  for (auto [bit, key] : mod_keys) {
    if (!(mods & bit)) continue;
    if (down)
      testing::input_injector::set_key_held(key);
    else
      testing::input_injector::set_key_up(key);
  }
}

// Press `chord` this frame and read the command it produces.
static std::optional<EditCommand> press(input::KeyChord chord) {
  testing::input_injector::reset_all();
  hold_mods(chord.required_modifiers, true);
  testing::input_injector::set_key_down(chord.key);
  testing::input_injector::reset_frame(); // the press lands next frame
  auto cmd = text_input::edit_command_from_keys();
  hold_mods(chord.required_modifiers, false);
  testing::input_injector::reset_all();
  return cmd;
}

int main() {
  check(!text_input::edit_command_from_keys(), "no keys, no command");
  check(press(chord_for(DefaultAction::TextBackspace)) == EditCommand::DeleteBack,
        "Backspace deletes back");
  check(press(chord_for(DefaultAction::TextDeleteWordBack)) ==
            EditCommand::DeleteWordBack,
        "the word modifier plus Backspace deletes a word");
  check(press(chord_for(DefaultAction::TextUndo)) == EditCommand::Undo,
        "the primary modifier plus Z undoes");
  check(press(chord_for(DefaultAction::TextRedo)) == EditCommand::Redo,
        "primary plus Shift plus Z redoes, not undoes");
  check(press(chord_for(DefaultAction::TextPaste)) == EditCommand::Paste,
        "primary plus V pastes");
  check(!press(input::KeyChord{keys::Z, 0}),
        "Z with no modifier is not an edit command");

  {
    enum class BareAction { None, WidgetMod, WidgetNext, WidgetBack, WidgetPress };
    static std::string logged;
    log_sink_fn = [](const char *, const char *message) { logged += message; };
    ui::warn_missing_text_actions<BareAction>();
    log_sink_fn = nullptr;
    check(logged.find("compiled out") == std::string::npos,
          "AFTERHOURS_NO_IMM_TEXT_INPUT silences the missing Text* warning");
  }

  printf("edit_keys_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
