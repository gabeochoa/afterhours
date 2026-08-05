// text_selection_test.cpp
// Selection geometry over a multi-line text block.
//
// These run under the RECORDING backend, which is the point of the header
// taking a measure callable rather than a Font. text_input's own caret and
// selection code cannot be tested at all: it is gated on a FontManager
// singleton, the raylib harness has no way to build one, and registering a real
// one segfaults with no window. Injected measurement side-steps that entirely.
//
// measure = chars * 10, so every offset below is arithmetic rather than a
// number read back off the implementation.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_selection.h>

#include <cstdio>
#include <string>

using namespace afterhours;
using namespace afterhours::ui;
namespace ts = afterhours::ui::text_selection;
namespace uid = afterhours::ui::detail;

namespace {
constexpr float CHAR_W = 10.f;
constexpr float LINE_H = 20.f;

// Ten pixels per byte. Only correct for ASCII, which is all these cases use.
float measure(const std::string &s) {
  return static_cast<float>(s.size()) * CHAR_W;
}

// wrap_runs_to_width measures per weight. These cases are all regular, so the
// weight is ignored and the width matches `measure` above.
float measure_w(const std::string &s, afterhours::colors::FontWeight) {
  return measure(s);
}

const Color kWhite{255, 255, 255, 255};
const Color kRed{255, 0, 0, 255};

// Hard breaks only -- a huge max_width means nothing soft-wraps, so the joined
// text is byte-identical to the source. This is the diff-viewer shape.
std::vector<uid::TextRunLine> hard_lines(const std::string &text) {
  return uid::wrap_runs_to_width({TextSpan{text, kWhite}}, 1e9f, measure_w);
}
} // namespace

TEST(joined_text_round_trips_hard_breaks) {
  const std::string src = "abc\ndefgh\nij";
  const auto lines = hard_lines(src);
  ui_test::check(lines.size() == 3, "three lines", __FILE__, __LINE__);
  ui_test::check(ts::joined_text(lines) == src,
                 "joined text equals the source", __FILE__, __LINE__);
}

TEST(line_start_offsets_account_for_the_newline) {
  // "abc\ndefgh\nij" -> starts at 0, 4, 10
  const auto lines = hard_lines("abc\ndefgh\nij");
  const auto starts = ts::line_start_offsets(lines);
  ui_test::check(starts.size() == 3, "three starts", __FILE__, __LINE__);
  if (starts.size() != 3)
    return;
  ui_test::check(starts[0] == 0 && starts[1] == 4 && starts[2] == 10,
                 "offsets skip the newline byte", __FILE__, __LINE__);
  fprintf(stderr, "        starts = %zu %zu %zu\n", starts[0], starts[1],
          starts[2]);
}

TEST(byte_offset_at_maps_a_point_to_a_byte) {
  const auto lines = hard_lines("abc\ndefgh\nij");

  // Row 1 ("defgh", starting at byte 4), 3 characters in -> 4 + 3 = 7.
  const size_t mid =
      ts::byte_offset_at(lines, Vector2Type{3.f * CHAR_W, 1.5f * LINE_H},
                         LINE_H, measure);
  ui_test::check(mid == 7, "row 1, 3 chars in, is byte 7", __FILE__, __LINE__);

  // Left of the first character on row 0.
  const size_t head =
      ts::byte_offset_at(lines, Vector2Type{0.f, 0.f}, LINE_H, measure);
  ui_test::check(head == 0, "top left is byte 0", __FILE__, __LINE__);

  fprintf(stderr, "        mid=%zu head=%zu\n", mid, head);
}

// A drag that runs off the bottom or the right should land on the nearest real
// position rather than nowhere.
TEST(byte_offset_at_clamps_outside_the_block) {
  const auto lines = hard_lines("abc\ndefgh\nij");

  const size_t below = ts::byte_offset_at(
      lines, Vector2Type{999.f, 99.f * LINE_H}, LINE_H, measure);
  ui_test::check(below == 12, "past the bottom right clamps to the end",
                 __FILE__, __LINE__);

  const size_t above = ts::byte_offset_at(lines, Vector2Type{0.f, -50.f},
                                          LINE_H, measure);
  ui_test::check(above == 0, "above the top clamps to the start", __FILE__,
                 __LINE__);
  fprintf(stderr, "        below=%zu above=%zu\n", below, above);
}

TEST(selection_within_one_line_is_one_rect) {
  const auto lines = hard_lines("abc\ndefgh\nij");
  // Bytes 5..7 = "ef" on row 1.
  const auto rects = ts::selection_rects(lines, ts::Range{5, 7},
                                         Vector2Type{0.f, 0.f}, LINE_H,
                                         measure);
  ui_test::check(rects.size() == 1, "one rect", __FILE__, __LINE__);
  if (rects.size() != 1)
    return;
  // "d" precedes it on that line, so x starts one char in and spans two.
  CHECK_APPROX(rects[0].x, 1.f * CHAR_W);
  CHECK_APPROX(rects[0].width, 2.f * CHAR_W);
  CHECK_APPROX(rects[0].y, 1.f * LINE_H);
}

TEST(selection_spanning_three_lines_is_three_rects) {
  const auto lines = hard_lines("abc\ndefgh\nij");
  // Byte 2 (mid row 0) through byte 11 (mid row 2).
  const auto rects = ts::selection_rects(lines, ts::Range{2, 11},
                                         Vector2Type{0.f, 0.f}, LINE_H,
                                         measure);
  ui_test::check(rects.size() == 3, "three rects", __FILE__, __LINE__);
  if (rects.size() != 3)
    return;

  // First is partial: from "c" to the end of row 0.
  CHECK_APPROX(rects[0].x, 2.f * CHAR_W);
  CHECK_APPROX(rects[0].width, 1.f * CHAR_W);
  // Middle is the whole of row 1.
  CHECK_APPROX(rects[1].x, 0.f);
  CHECK_APPROX(rects[1].width, 5.f * CHAR_W);
  CHECK_APPROX(rects[1].y, 1.f * LINE_H);
  // Last is partial: "i" only.
  CHECK_APPROX(rects[2].x, 0.f);
  CHECK_APPROX(rects[2].width, 1.f * CHAR_W);
  CHECK_APPROX(rects[2].y, 2.f * LINE_H);
}

TEST(selection_rects_honour_the_origin) {
  const auto lines = hard_lines("abc\ndef");
  const auto rects = ts::selection_rects(lines, ts::Range{0, 2},
                                         Vector2Type{100.f, 50.f}, LINE_H,
                                         measure);
  ui_test::check(rects.size() == 1, "one rect", __FILE__, __LINE__);
  if (rects.size() == 1) {
    CHECK_APPROX(rects[0].x, 100.f);
    CHECK_APPROX(rects[0].y, 50.f);
  }
}

// A range ending exactly on the break must not produce an empty rect for the
// next line -- the newline is not a drawn character.
TEST(a_range_ending_on_a_line_break_covers_only_that_line) {
  const auto lines = hard_lines("abc\ndef");
  const auto rects = ts::selection_rects(lines, ts::Range{0, 3},
                                         Vector2Type{0.f, 0.f}, LINE_H,
                                         measure);
  ui_test::check(rects.size() == 1, "one rect, not two", __FILE__, __LINE__);
  fprintf(stderr, "        rects=%zu\n", rects.size());
}

TEST(an_empty_range_draws_nothing) {
  const auto lines = hard_lines("abc\ndef");
  const auto rects = ts::selection_rects(lines, ts::Range{4, 4},
                                         Vector2Type{0.f, 0.f}, LINE_H,
                                         measure);
  ui_test::check(rects.empty(), "no rects for a caret-only selection",
                 __FILE__, __LINE__);
}

TEST(substring_returns_the_selected_text) {
  const auto lines = hard_lines("abc\ndefgh\nij");
  ui_test::check(ts::substring(lines, ts::Range{5, 7}) == "ef",
                 "within a line", __FILE__, __LINE__);
  // Across a break the newline comes with it, which is what a copy should give.
  ui_test::check(ts::substring(lines, ts::Range{2, 6}) == "c\nde",
                 "across a line break", __FILE__, __LINE__);
  ui_test::check(ts::substring(lines, ts::Range{3, 3}).empty(),
                 "empty range yields nothing", __FILE__, __LINE__);
}

TEST(selection_tracks_drag_direction) {
  ts::Selection sel;
  sel.collapse_to(10);
  ui_test::check(!sel.has_selection(), "a collapsed selection is none",
                 __FILE__, __LINE__);
  // Dragging backwards: cursor ends up before the anchor.
  sel.set_cursor(4, /*extend=*/true);
  ui_test::check(sel.has_selection(), "dragging creates a selection", __FILE__,
                 __LINE__);
  ui_test::check(sel.range().start == 4 && sel.range().end == 10,
                 "range is ordered regardless of drag direction", __FILE__,
                 __LINE__);
}

// The multi-colour case: a diff line is red or green, and selection geometry
// must not care where one colour stops and the next starts.
TEST(runs_of_different_colours_are_one_continuous_line) {
  const std::vector<TextSpan> runs{TextSpan{"red", kRed},
                                   TextSpan{"white", kWhite}};
  const auto lines = uid::wrap_runs_to_width(runs, 1e9f, measure_w);
  ui_test::check(lines.size() == 1, "one visual line", __FILE__, __LINE__);
  fprintf(stderr, "        joined = '%s'\n", ts::joined_text(lines).c_str());
  ui_test::check(ts::joined_text(lines) == "redwhite",
                 "colour boundaries do not add characters", __FILE__, __LINE__);

  const auto rects = ts::selection_rects(lines, ts::Range{1, 5},
                                         Vector2Type{0.f, 0.f}, LINE_H,
                                         measure);
  ui_test::check(rects.size() == 1, "a selection across colours is one rect",
                 __FILE__, __LINE__);
  if (rects.size() == 1) {
    CHECK_APPROX(rects[0].x, 1.f * CHAR_W);
    CHECK_APPROX(rects[0].width, 4.f * CHAR_W);
  }
}

int main() { return ui_test::run_registered_tests("text selection"); }
