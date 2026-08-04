// text_wrap_test.cpp
// Regression tests for word-wrap of static labels (TextOverflow::Wrap).
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/text_wrap_test.cpp -o /tmp/t && /tmp/t
//
// Exercises the greedy word-wrap helper used by both render paths
// (RenderImm and RenderBatched) in rendering.h. Uses a deterministic measure
// function (width = chars * 10) so the expected line breaks are exact.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/rendering.h>

using afterhours::ui::detail::measure_wrapped;
using afterhours::ui::detail::wrap_text_to_width;

// Fixed-width measure: every character is 10px wide.
static auto measure10 = [](const std::string &s) {
  return static_cast<float>(s.size()) * 10.f;
};

// 2D variant: width = chars * 10, height = 20 per line.
static auto measure10x20 = [](const std::string &s) {
  return Vector2Type{static_cast<float>(s.size()) * 10.f, 20.f};
};

// Text that fits stays on one line.
TEST(wrap_fits_single_line) {
  auto lines = wrap_text_to_width("hello world", 1000.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "hello world");
}

// Text longer than the width breaks on spaces (greedy).
TEST(wrap_breaks_on_word_boundary) {
  // "aaa bbb ccc" = 11 chars = 110px. Width 70 fits "aaa bbb" (7 -> 70) but
  // not "aaa bbb ccc", so it breaks before "ccc".
  auto lines = wrap_text_to_width("aaa bbb ccc", 70.f, measure10);
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "aaa bbb");
  CHECK(lines[1] == "ccc");
}

// Each word goes on its own line when the width only fits one word.
TEST(wrap_one_word_per_line) {
  auto lines = wrap_text_to_width("one two three", 50.f, measure10);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "one");
  CHECK(lines[1] == "two");
  CHECK(lines[2] == "three");
}

// A single word wider than the width is not character-split; it occupies its
// own (overflowing) line.
TEST(wrap_long_word_not_split) {
  auto lines = wrap_text_to_width("supercalifragilistic", 50.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "supercalifragilistic");
}

// Empty text yields a single empty line (never zero lines).
TEST(wrap_empty_text) {
  auto lines = wrap_text_to_width("", 100.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0].empty());
}

// Non-positive width degrades gracefully to a single line.
TEST(wrap_zero_width) {
  auto lines = wrap_text_to_width("a b c", 0.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "a b c");
}

// A realistic sentence wraps into the expected number of lines for a fixed box.
TEST(wrap_sentence_multiple_lines) {
  // 250px wide box. "Configure vibration, save data, and autosave settings."
  auto lines = wrap_text_to_width(
      "Configure vibration, save data, and autosave settings.", 250.f,
      measure10);
  CHECK(lines.size() >= 2); // does not fit on one line
  // No line exceeds the width.
  for (const auto &ln : lines)
    CHECK(measure10(ln) <= 250.f);
}

// Hard newlines always break a line, even when the text would otherwise fit.
TEST(wrap_hard_newline_breaks) {
  auto lines = wrap_text_to_width("alpha\nbeta", 1000.f, measure10);
  CHECK(lines.size() == 2);
  CHECK(lines[0] == "alpha");
  CHECK(lines[1] == "beta");
}

// A blank line between two hard newlines is preserved.
TEST(wrap_blank_line_preserved) {
  auto lines = wrap_text_to_width("a\n\nb", 1000.f, measure10);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "a");
  CHECK(lines[1].empty());
  CHECK(lines[2] == "b");
}

// Hard newlines and greedy word-wrap combine: each segment wraps independently.
TEST(wrap_newline_plus_wordwrap) {
  // "aaa bbb ccc" wraps to 2 lines at width 70; the "\nddd" forces a 3rd.
  auto lines = wrap_text_to_width("aaa bbb ccc\nddd", 70.f, measure10);
  CHECK(lines.size() == 3);
  CHECK(lines[0] == "aaa bbb");
  CHECK(lines[1] == "ccc");
  CHECK(lines[2] == "ddd");
}

// Leading whitespace is indentation, not padding to be tidied away. Code and
// diff views put text through here, and collapsing runs of spaces silently
// reindents them.
TEST(wrap_preserves_leading_indentation) {
  auto lines = wrap_text_to_width("    indented", 1000.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "    indented");
}

// Runs of spaces inside a line are kept verbatim, so column alignment holds.
TEST(wrap_preserves_runs_of_spaces) {
  auto lines = wrap_text_to_width("a    b", 1000.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "a    b");
}

// Indentation after a hard break survives too -- every line of a code block
// past the first depends on this.
TEST(wrap_preserves_indentation_after_a_hard_break) {
  auto lines = wrap_text_to_width("fn main() {\n    body();\n}", 1000.f,
                                  measure10);
  CHECK(lines.size() == 3);
  if (lines.size() != 3)
    return;
  CHECK(lines[1] == "    body();");
}

// Trailing spaces are kept, so hard-broken text round-trips byte for byte
// (text_selection's offsets index into exactly this text).
TEST(wrap_preserves_trailing_spaces) {
  auto lines = wrap_text_to_width("trailing   ", 1000.f, measure10);
  CHECK(lines.size() == 1);
  CHECK(lines[0] == "trailing   ");
}

// The space a soft wrap breaks at IS consumed -- a wrapped line must not start
// with the separator it broke on.
TEST(wrap_consumes_whitespace_at_the_break) {
  // "aaa bbb ccc" at width 70 breaks before "ccc".
  auto lines = wrap_text_to_width("aaa bbb ccc", 70.f, measure10);
  CHECK(lines.size() == 2);
  if (lines.size() != 2)
    return;
  CHECK(lines[1] == "ccc"); // not " ccc"
}

// measure_wrapped reports width (widest line), height (lines * line height),
// and line count.
TEST(measure_wrapped_metrics) {
  auto m = measure_wrapped("one two three", 50.f, measure10x20);
  CHECK(m.line_count == 3);       // one / two / three
  CHECK(m.height == 60.f);        // 3 lines * 20px
  CHECK(m.width == 50.f);         // "three" = 5 chars * 10
}

// measure_wrapped counts blank lines from hard newlines in the height.
TEST(measure_wrapped_counts_blank_lines) {
  auto m = measure_wrapped("a\n\nb", 1000.f, measure10x20);
  CHECK(m.line_count == 3);
  CHECK(m.height == 60.f);        // blank middle line still occupies height
}

int main() { return ui_test::run_registered_tests("text wrap tests"); }
