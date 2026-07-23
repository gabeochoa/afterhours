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

using afterhours::ui::detail::wrap_text_to_width;

// Fixed-width measure: every character is 10px wide.
static auto measure10 = [](const std::string &s) {
  return static_cast<float>(s.size()) * 10.f;
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

int main() { return ui_test::run_registered_tests("text wrap tests"); }
