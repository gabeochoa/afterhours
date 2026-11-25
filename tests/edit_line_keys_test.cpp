// edit_line_keys_test.cpp
// text_input::edit_line_from_keys and friends: cursor motion, selection and
// typed characters for a hand-drawn field, driven headlessly through fake
// key callbacks so no window is needed.

#define AFTERHOURS_NO_IMM_TEXT_INPUT

#include <cstdio>
#include <queue>
#include <set>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui/text_input/edit_keys.h>

using namespace afterhours;
using namespace afterhours::text_input;

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

struct FakeKeys {
  std::set<int> pressed;
  std::set<int> down;
  std::queue<int> chars;

  LineEditKeys make() {
    LineEditKeys keys;
    keys.is_pressed = [this](int key) { return pressed.count(key) > 0; };
    keys.is_down = [this](int key) { return down.count(key) > 0; };
    keys.get_char = [this]() {
      if (chars.empty())
        return 0;
      int cp = chars.front();
      chars.pop();
      return cp;
    };
    return keys;
  }

  void type(const std::string &text) {
    for (unsigned char c : text)
      chars.push(c);
  }
};

static HasTextInputState field(const std::string &text, size_t cursor) {
  HasTextInputState state;
  state.storage.insert(0, text);
  state.cursor_position = cursor;
  return state;
}

#if defined(__APPLE__)
static constexpr int PRIMARY_KEY = keys::LEFT_SUPER;
#else
static constexpr int PRIMARY_KEY = keys::LEFT_CONTROL;
#endif

int main() {
  {
    auto state = field("", 0);
    FakeKeys fake;
    fake.type("hi");
    check(edit_line_from_keys(state, fake.make()), "typing reports a change");
    check(state.text() == "hi", "typed ASCII lands in the field");
    check(state.cursor_position == 2, "cursor follows typed text");
  }

  {
    auto state = field("", 0);
    FakeKeys fake;
    fake.chars.push(0xE9);
    fake.chars.push(0x20AC);
    check(insert_typed_chars(state, fake.make()), "UTF-8 typing reports a change");
    check(state.text() == "\xC3\xA9\xE2\x82\xAC",
          "multi-byte codepoints are UTF-8 encoded");
    check(state.cursor_position == 5, "cursor advances by bytes, not codepoints");
  }

  {
    auto state = field("", 0);
    FakeKeys fake;
    fake.chars.push(8);
    fake.chars.push(31);
    fake.chars.push('a');
    insert_typed_chars(state, fake.make());
    check(state.text() == "a", "control codepoints are ignored");
  }

  {
    auto state = field("abc", 3);
    FakeKeys fake;
    fake.pressed = {keys::LEFT};
    check(motion_from_keys(state, fake.make()), "Left reports a change");
    check(state.cursor_position == 2, "Left moves the cursor back one char");
    fake.pressed = {keys::RIGHT};
    motion_from_keys(state, fake.make());
    check(state.cursor_position == 3, "Right moves the cursor forward");
    fake.pressed = {keys::HOME};
    motion_from_keys(state, fake.make());
    check(state.cursor_position == 0, "Home jumps to the start");
    fake.pressed = {keys::END};
    motion_from_keys(state, fake.make());
    check(state.cursor_position == 3, "End jumps to the end");
  }

  {
    auto state = field("hello", 5);
    FakeKeys fake;
    fake.down = {keys::LEFT_SHIFT};
    fake.pressed = {keys::LEFT};
    motion_from_keys(state, fake.make());
    check(state.has_selection(), "Shift+Left starts a selection");
    check(state.selected_text() == "o", "the selection covers the last char");
    FakeKeys typing;
    typing.type("y");
    edit_line_from_keys(state, typing.make());
    check(state.text() == "helly", "typing replaces the selection");
    check(!state.has_selection(), "the selection collapses after typing");
  }

  {
    auto state = field("hello world", 11);
    FakeKeys fake;
    fake.down = {keys::LEFT_CONTROL};
    fake.pressed = {keys::LEFT};
    motion_from_keys(state, fake.make());
    check(state.cursor_position == 6, "word jump lands on the word start");
    fake.down = {keys::LEFT_CONTROL};
    fake.pressed = {keys::BACKSPACE};
    FakeKeys del;
    del.down = {keys::LEFT_CONTROL};
    del.pressed = {keys::BACKSPACE};
    auto fresh = field("hello world", 11);
    motion_from_keys(fresh, del.make());
    check(fresh.text() == "hello ", "word Backspace deletes the word");
    check(fresh.cursor_position == 6, "cursor follows the word deletion");
  }

  {
    auto state = field("abc", 2);
    FakeKeys fake;
    fake.pressed = {keys::BACKSPACE};
    motion_from_keys(state, fake.make());
    check(state.text() == "ac", "Backspace deletes before the cursor");
    fake.pressed = {keys::DELETE_KEY};
    motion_from_keys(state, fake.make());
    check(state.text() == "a", "Delete removes at the cursor");
  }

  {
    auto state = field("", 0);
    FakeKeys typing;
    typing.type("a");
    edit_line_from_keys(state, typing.make());
    check(state.text() == "a", "typed char is in place before undo");

    FakeKeys undo_keys;
    undo_keys.down = {PRIMARY_KEY};
    undo_keys.pressed = {keys::Z};
    edit_line_from_keys(state, undo_keys.make());
    check(state.text().empty(), "primary+Z undoes the typing");

    FakeKeys redo_keys;
    redo_keys.down = {PRIMARY_KEY, keys::LEFT_SHIFT};
    redo_keys.pressed = {keys::Z};
    edit_line_from_keys(state, redo_keys.make());
    check(state.text() == "a", "primary+Shift+Z redoes it");
  }

  printf("edit_line_keys_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
