// Line-level editing and the imperative responder surface.
//
// Word deletion existed; deleting to the start or end of the line did not,
// so macOS Cmd+Backspace / Ctrl+K had nothing to bind to (hanabi #257).
// Editing ops were also only reachable from the widgets' own key handling:
// an app could not send "delete word" to whichever field was focused
// (hanabi #565). And default_keymap bound Cmd and Ctrl for every chord,
// which is wrong on macOS, where word ops are Option, Cmd+Left is line
// start, and Cmd+Backspace is delete-to-line-start (hanabi #264).
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/ui.h>
#include <afterhours/src/plugins/ui/text_input/text_area_state.h>
#include <afterhours/src/plugins/ui/text_input/utils.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::text_input;

static int tests_run = 0;
static int tests_passed = 0;

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

static HasTextInputState field(const std::string &text, size_t cursor) {
  HasTextInputState s;
  s.storage.insert(0, text);
  s.cursor_position = cursor;
  return s;
}

static HasTextAreaState area(const std::string &text, size_t cursor) {
  HasTextAreaState s;
  s.storage.insert(0, text);
  s.rebuild_line_index();
  s.cursor_position = cursor;
  return s;
}

// ---- delete to line start / end (single-line: the line is the text) ----

static void test_field_delete_to_line_start() {
  auto s = field("hello world", 8);
  CHECK(delete_to_line_start(s));
  CHECK(s.text() == "rld");
  CHECK(s.cursor_position == 0);
}

static void test_field_delete_to_line_end() {
  auto s = field("hello world", 3);
  CHECK(delete_to_line_end(s));
  CHECK(s.text() == "hel");
  CHECK(s.cursor_position == 3);
}

static void test_line_delete_boundaries_are_noops() {
  auto s = field("hello", 0);
  CHECK(!delete_to_line_start(s));
  CHECK(s.text() == "hello");
  s.cursor_position = 5;
  CHECK(!delete_to_line_end(s));
  CHECK(s.text() == "hello");
}

// ---- the same ops on a text area stop at the source line's edges ----

static void test_area_delete_to_line_start_stops_at_newline() {
  auto s = area("one\ntwo three\nfour", 8); // inside "two three"
  CHECK(delete_to_line_start(s));
  CHECK(s.text() == "one\nthree\nfour");
  CHECK(s.cursor_position == 4);
}

static void test_area_delete_to_line_end_stops_at_newline() {
  auto s = area("one\ntwo three\nfour", 6);
  CHECK(delete_to_line_end(s));
  CHECK(s.text() == "one\ntw\nfour");
  CHECK(s.cursor_position == 6);
}

static void test_area_delete_at_line_edges_is_noop() {
  auto s = area("one\ntwo", 4); // start of line 2
  CHECK(!delete_to_line_start(s));
  s.cursor_position = 7; // end of text / line 2
  CHECK(!delete_to_line_end(s));
  CHECK(s.text() == "one\ntwo");
}

// ---- the responder: commands an app can send without a key ----

static void test_command_delete_line_back_and_undo() {
  auto s = field("hello world", 8);
  CHECK(apply_edit_command(s, EditCommand::DeleteLineBack));
  CHECK(s.text() == "rld");
  CHECK(apply_edit_command(s, EditCommand::Undo));
  CHECK(s.text() == "hello world");
}

static void test_command_takes_selection_first() {
  auto s = field("hello world", 5);
  s.selection_anchor = 8; // [5,8) = " wo"
  CHECK(apply_edit_command(s, EditCommand::DeleteLineBack));
  CHECK(s.text() == "hellorld");
  CHECK(!s.has_selection());
}

static void test_command_word_and_char_delete() {
  auto s = field("one two three", 7); // caret after "one two"
  CHECK(apply_edit_command(s, EditCommand::DeleteWordBack));
  // Word deletion leaves both surrounding spaces; the forward delete
  // then removes one of them.
  CHECK(s.text() == "one  three");
  CHECK(apply_edit_command(s, EditCommand::DeleteForward));
  CHECK(s.text() == "one three");
}

static void test_command_respects_readonly() {
  auto s = field("hello", 3);
  s.readonly = true;
  CHECK(!apply_edit_command(s, EditCommand::DeleteLineBack));
  CHECK(s.text() == "hello");
  // Selection is not a mutation.
  CHECK(apply_edit_command(s, EditCommand::SelectAll));
  CHECK(s.has_selection());
}

// ---- dispatch onto an entity carrying either state type ----

static void reset_world() {
  ui::UICollectionHolder::get().collection
      .delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

static void test_command_dispatches_to_area_entity_and_rebuilds_index() {
  reset_world();
  Entity &e = ui::UICollectionHolder::get().collection.createEntity();
  ui::UICollectionHolder::get().collection.merge_entity_arrays();
  auto &s = e.addComponent<HasTextAreaState>();
  s.storage.insert(0, "one\ntwo three");
  s.rebuild_line_index();
  s.cursor_position = 8;

  CHECK(apply_edit_command_to_entity(e, EditCommand::DeleteLineBack));
  CHECK(s.text() == "one\nthree");
  CHECK(s.line_count() == 2); // index rebuilt, not stale
}

static void test_command_dispatches_to_field_entity() {
  reset_world();
  Entity &e = ui::UICollectionHolder::get().collection.createEntity();
  ui::UICollectionHolder::get().collection.merge_entity_arrays();
  auto &s = e.addComponent<HasTextInputState>();
  s.storage.insert(0, "hello world");
  s.cursor_position = 8;

  CHECK(apply_edit_command_to_entity(e, EditCommand::DeleteLineForward));
  CHECK(s.text() == "hello wo");
}

static void test_command_on_entity_without_state_is_false() {
  reset_world();
  Entity &e = ui::UICollectionHolder::get().collection.createEntity();
  ui::UICollectionHolder::get().collection.merge_entity_arrays();
  CHECK(!apply_edit_command_to_entity(e, EditCommand::DeleteLineBack));
}

// ---- the default keymap is platform-correct ----

using GameMapping = input::ProvidesInputMapping::GameMapping;

static bool has_chord(const GameMapping &m, ui::DefaultAction action, int key,
                      uint8_t mods) {
  auto it = m.find(static_cast<int>(action));
  if (it == m.end())
    return false;
  for (const auto &input : it->second) {
    if (const auto *chord = std::get_if<input::KeyChord>(&input))
      if (chord->key == key && chord->required_modifiers == mods)
        return true;
  }
  return false;
}

static void test_default_keymap_line_and_word_chords() {
  const GameMapping m = ui::default_keymap<ui::DefaultAction>();
  using KC = input::KeyChord;
#if defined(__APPLE__)
  // macOS: word ops are Option, never Cmd; line ops are Cmd / Ctrl+K.
  CHECK(has_chord(m, ui::DefaultAction::TextDeleteWordBack, keys::BACKSPACE,
                  KC::MOD_ALT));
  CHECK(!has_chord(m, ui::DefaultAction::TextDeleteWordBack, keys::BACKSPACE,
                   KC::MOD_SUPER));
  CHECK(has_chord(m, ui::DefaultAction::TextDeleteLineBack, keys::BACKSPACE,
                  KC::MOD_SUPER));
  CHECK(has_chord(m, ui::DefaultAction::TextDeleteLineForward, keys::K,
                  KC::MOD_CTRL));
  CHECK(has_chord(m, ui::DefaultAction::TextHome, keys::LEFT, KC::MOD_SUPER));
  CHECK(has_chord(m, ui::DefaultAction::TextEnd, keys::RIGHT, KC::MOD_SUPER));
  CHECK(has_chord(m, ui::DefaultAction::TextWordLeft, keys::LEFT, KC::MOD_ALT));
  CHECK(!has_chord(m, ui::DefaultAction::TextWordLeft, keys::LEFT,
                   KC::MOD_SUPER));
#else
  // Elsewhere: word ops are Ctrl; line delete is Ctrl+Shift+Backspace and
  // Ctrl+K is not a platform convention, so it stays unbound.
  CHECK(has_chord(m, ui::DefaultAction::TextDeleteWordBack, keys::BACKSPACE,
                  KC::MOD_CTRL));
  CHECK(has_chord(m, ui::DefaultAction::TextDeleteLineBack, keys::BACKSPACE,
                  static_cast<uint8_t>(KC::MOD_CTRL | KC::MOD_SHIFT)));
  CHECK(!has_chord(m, ui::DefaultAction::TextDeleteLineForward, keys::K,
                   KC::MOD_CTRL));
#endif
  // Select-all / clipboard keep their per-platform modifier either way.
#if defined(__APPLE__)
  CHECK(has_chord(m, ui::DefaultAction::TextSelectAll, keys::A, KC::MOD_SUPER));
#else
  CHECK(has_chord(m, ui::DefaultAction::TextSelectAll, keys::A, KC::MOD_CTRL));
#endif
}

int main() {
  printf("Running text line edit tests...\n\n");
  printf("  field_delete_to_line_start\n");
  test_field_delete_to_line_start();
  printf("  field_delete_to_line_end\n");
  test_field_delete_to_line_end();
  printf("  line_delete_boundaries_are_noops\n");
  test_line_delete_boundaries_are_noops();
  printf("  area_delete_to_line_start_stops_at_newline\n");
  test_area_delete_to_line_start_stops_at_newline();
  printf("  area_delete_to_line_end_stops_at_newline\n");
  test_area_delete_to_line_end_stops_at_newline();
  printf("  area_delete_at_line_edges_is_noop\n");
  test_area_delete_at_line_edges_is_noop();
  printf("  command_delete_line_back_and_undo\n");
  test_command_delete_line_back_and_undo();
  printf("  command_takes_selection_first\n");
  test_command_takes_selection_first();
  printf("  command_word_and_char_delete\n");
  test_command_word_and_char_delete();
  printf("  command_respects_readonly\n");
  test_command_respects_readonly();
  printf("  command_dispatches_to_area_entity_and_rebuilds_index\n");
  test_command_dispatches_to_area_entity_and_rebuilds_index();
  printf("  command_dispatches_to_field_entity\n");
  test_command_dispatches_to_field_entity();
  printf("  command_on_entity_without_state_is_false\n");
  test_command_on_entity_without_state_is_false();
  printf("  default_keymap_line_and_word_chords\n");
  test_default_keymap_line_and_word_chords();

  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
