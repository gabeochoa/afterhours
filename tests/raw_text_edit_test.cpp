#define AFTERHOURS_NO_IMM_TEXT_INPUT

#include <cstdio>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/e2e_testing/test_input.h>
#include <afterhours/src/plugins/ui/text_input/raw_text_edit.h>

using namespace afterhours;
using text_input::RawTextEdit;
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

static void press_frame(RawTextEdit &f, input::KeyChord chord) {
  testing::test_input::reset_all();
  hold_mods(chord.required_modifiers, true);
  testing::input_injector::set_key_down(chord.key);
  testing::input_injector::reset_frame();
  text_input::update_raw_text_edit(f, 0.016f);
  hold_mods(chord.required_modifiers, false);
  testing::test_input::reset_all();
}

static void press_frame(RawTextEdit &f, DefaultAction action) {
  press_frame(f, chord_for(action));
}

static void move_frame(RawTextEdit &f, int key, bool shift = false,
                       bool word_mod = false) {
  testing::test_input::reset_all();
  if (shift) testing::input_injector::set_key_held(keys::LEFT_SHIFT);
#if defined(__APPLE__)
  if (word_mod) testing::input_injector::set_key_held(keys::LEFT_ALT);
#else
  if (word_mod) testing::input_injector::set_key_held(keys::LEFT_CONTROL);
#endif
  testing::input_injector::set_key_down(key);
  testing::input_injector::reset_frame();
  text_input::update_raw_text_edit(f, 0.016f);
  testing::test_input::reset_all();
}

static void type_frame(RawTextEdit &f, const char *text) {
  testing::test_input::reset_all();
  testing::test_input::push_text(text);
  text_input::update_raw_text_edit(f, 0.016f);
  testing::test_input::reset_all();
}

int main() {
  testing::test_input::detail::test_mode = true;
  {
    RawTextEdit f;
    type_frame(f, "hello world");
    check(f.text() == "hello world", "typed chars land in order");
    check(f.cursor_position == f.text_size(), "caret ends after the text");

    press_frame(f, DefaultAction::TextBackspace);
    check(f.text() == "hello worl", "backspace deletes before the caret");

    press_frame(f, DefaultAction::TextDeleteWordBack);
    check(f.text() == "hello ", "word-backspace deletes the word");

    move_frame(f, keys::HOME);
    check(f.cursor_position == 0, "home moves to the start");
    press_frame(f, DefaultAction::TextDelete);
    check(f.text() == "ello ", "delete removes the char at the caret");

    move_frame(f, keys::END);
    type_frame(f, "!");
    check(f.text() == "ello !", "typing at end appends");

    move_frame(f, keys::LEFT, false, true);
    check(f.cursor_position == 0, "word-left lands on the word start");
    move_frame(f, keys::RIGHT, false, true);
    check(f.cursor_position == 4, "word-right lands on the word end");
    move_frame(f, keys::LEFT);
    check(f.cursor_position == 3, "left moves one grapheme");
    move_frame(f, keys::RIGHT, true);
    check(f.has_selection() && f.selected_text() == "o",
          "shift-right extends the selection");
    type_frame(f, "_");
    check(f.text() == "ell_ !", "typing replaces the selection");
  }

  {
    clipboard::MemoryProvider memory;
    clipboard::ScopedProvider scope(memory);
    RawTextEdit f{"old"};
    press_frame(f, DefaultAction::TextSelectAll);
    check(f.selected_text() == "old", "select-all selects everything");
    clipboard::set_text("new\nline");
    press_frame(f, DefaultAction::TextPaste);
    check(f.text() == "new line",
          "paste replaces the selection, newline becomes a space");
    check(f.cursor_position == f.text_size(), "caret ends after the paste");

    clipboard::set_text("more");
    press_frame(f, DefaultAction::TextPaste);
    check(f.text() == "new linemore", "paste at the caret inserts");
  }

  {
    RawTextEdit f;
    check(text_input::raw_text_cursor_visible(f), "caret starts visible");
    text_input::update_raw_text_edit(f, 1.0f);
    check(!text_input::raw_text_cursor_visible(f),
          "caret hides in the second blink half-cycle");
    f.readonly = true;
    type_frame(f, "nope");
    check(f.text().empty(), "readonly drains the char queue without editing");
  }

  printf("raw_text_edit_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
