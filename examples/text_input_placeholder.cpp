// text_input_placeholder.cpp
// A standing repro for the placeholder crash, and a demo of the feature.
//
// THE BUG THIS EXISTS TO PREVENT
//
// `text_input` shows `config.placeholder` by parking it in the field's
// `HasLabel`. Click-to-position then measured THAT label to decide where the
// caret goes -- correct for a masked field, where the label carries the mask,
// and wrong for an empty one, where the label carries the hint. So:
//
//     1. a field is empty, so it renders "Search conversations"
//     2. the user clicks near the end of that hint
//     3. cursor_position becomes ~20, an offset into text nobody typed
//     4. the user types one character
//     5. insert_char calls std::string::insert(20, "h") on ""
//     6. std::string::insert THROWS out_of_range rather than clamping
//     7. SIGABRT
//
// Every step is what a first-time user of the feature does. It was found by
// adopting `with_placeholder` in a downstream app, whose test suite went from
// green to four simultaneous SIGABRTs.
//
// Two fixes, both exercised below: the click path treats an empty field as
// having exactly one caret position (0) instead of measuring the hint, and
// insert_char clamps the cursor to the text length before inserting, so no
// route to a stale cursor can abort a process over a keystroke.
//
// WHY AN EXAMPLE AND NOT ONLY A UNIT TEST
//
// There was already a placeholder test -- `d12_placeholder_does_not_move_the
// _cursor` -- and it passed throughout, because it checks where the caret
// RENDERS. The caret drew in exactly the right place; the damage was in what
// the click wrote to `cursor_position`, which nothing looked at until a
// keystroke arrived. The scenario needs the whole click-then-type sequence to
// show itself, so it lives here as a runnable one.
//
// Build:
//   cd examples && make text_input_placeholder
//
// Run:
//   ./text_input_placeholder            # exits 0 when the field survives
//
// Exit codes: 0 all scenarios survived, 1 a scenario misbehaved. The crashing
// build does not reach either -- it aborts, which is the point.

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/src/plugins/ui/text_input/utils.h>

#include <cstdio>
#include <string>

using afterhours::text_input::HasTextInputState;
using afterhours::text_input::insert_char;

namespace {

int g_failures = 0;

void check(bool ok, const std::string &what) {
  std::printf("  %-58s %s\n", what.c_str(), ok ? "ok" : "FAILED");
  if (!ok) ++g_failures;
}

// The exact sequence from the bug report: an empty field showing a hint, a
// click that lands inside the hint, then one keystroke.
void scenario_click_the_hint_then_type() {
  std::printf("click inside a placeholder, then type\n");

  HasTextInputState s;
  // Where a click measured against "Search conversations" used to land. The
  // field itself is empty -- nobody typed those twenty characters.
  s.cursor_position = 20;

  // Before the fix this line did not return. It threw out_of_range from
  // std::string::insert and the process aborted.
  const bool inserted = insert_char(s, 'h');

  check(inserted, "the keystroke is accepted");
  check(s.text() == "h", "the field holds exactly what was typed");
  check(s.cursor_position == 1, "the caret sits after the character");
}

// The same hazard by a different route, and the reason insert_char clamps
// rather than the click path alone being fixed: a host that shortens the bound
// string between frames leaves the cursor pointing past the end.
void scenario_host_shortened_the_text() {
  std::printf("host clears the bound string while the caret is deep in it\n");

  HasTextInputState s;
  s.storage.insert(0, "a long draft the app is about to clear");
  s.cursor_position = 30;
  s.storage.clear();  // e.g. the draft was sent, or the thread was switched

  check(insert_char(s, 'x'), "the next keystroke is accepted");
  check(s.text() == "x", "and lands in the now-empty field");
}

// Clamping must go to the END, not the start: the caret appeared to be at the
// end of the text, so that is where the character belongs.
void scenario_clamp_lands_at_the_end() {
  std::printf("a cursor past the end clamps to the end, not the start\n");

  HasTextInputState s;
  s.storage.insert(0, "ab");
  s.cursor_position = 99;

  check(insert_char(s, 'c'), "the keystroke is accepted");
  check(s.text() == "abc", "appended, not prepended");
}

// The ordinary path, so a fix that broke normal typing would show up here too.
void scenario_normal_typing_is_unaffected() {
  std::printf("typing into a field with a sane cursor is unchanged\n");

  HasTextInputState s;
  for (char c : std::string("hi")) insert_char(s, c);
  check(s.text() == "hi", "characters arrive in order");

  s.cursor_position = 1;  // between h and i
  insert_char(s, '!');
  check(s.text() == "h!i", "an interior insert still goes where it is told");
}

}  // namespace

int main() {
  std::printf("text_input placeholder repro\n");
  std::printf("(a build without the fix ABORTS in the first scenario)\n\n");

  scenario_click_the_hint_then_type();
  scenario_host_shortened_the_text();
  scenario_clamp_lands_at_the_end();
  scenario_normal_typing_is_unaffected();

  std::printf("\n%s\n", g_failures == 0 ? "all scenarios survived"
                                        : "SOME SCENARIOS MISBEHAVED");
  return g_failures == 0 ? 0 : 1;
}
