// text_area_test.cpp
// D26: text_area never word-wrapped. It hand-split the text on '\n' and
// ignored the word_wrap config entirely, so a chat composer's long message ran
// off the right edge -- while a plain label with TextOverflow::Wrap, three
// files over, wrapped correctly through the shared primitive.
//
// The hard part of wrapping an EDITABLE block is not the breaking, it is the
// mapping: the cursor is a byte offset into the source, and a soft break
// consumes the space it broke at, so visual line k does not start at a
// position arithmetic can predict. TextLayoutCache owns that mapping and is
// what these pin.
//
// Recording backend with measure = chars * 10, so every offset below is
// arithmetic rather than a number read back off the implementation.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/ui/text_input/text_area.h>
#include <afterhours/src/plugins/ui/text_input/text_layout.h>

#include <string>
#include <string_view>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
namespace ti = afterhours::text_input;

namespace {
constexpr float CHAR_W = 10.f;
constexpr float LINE_H = 20.f;

float measure(std::string_view s) {
  return static_cast<float>(s.size()) * CHAR_W;
}

// Build a layout over `text`, wrapped to `chars` characters' worth of width.
ti::TextLayoutCache layout(const std::string &text, float chars) {
  ti::TextLayoutCache c;
  c.rebuild(text, chars * CHAR_W, LINE_H, measure);
  return c;
}

// The text each visual line covers, sliced back out of the source. If the
// offsets are wrong this is the assertion that shows it.
std::string at(const ti::TextLayoutCache &c, const std::string &src,
               size_t i) {
  return c.line_text(src, i);
}
} // namespace

// ---------------------------------------------------------------------------
// Breaking
// ---------------------------------------------------------------------------

TEST(no_wrap_needed_is_one_line) {
  const std::string src = "hello";
  auto c = layout(src, 20.f);
  CHECK(c.line_count() == 1);
  CHECK(at(c, src, 0) == "hello");
  CHECK_APPROX(c.total_height(), LINE_H);
}

// The whole point: a long line breaks at a space instead of running off.
TEST(a_long_line_soft_wraps) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f); // "hello world" is exactly 11
  CHECK(c.line_count() == 2);
  CHECK(at(c, src, 0) == "hello world");
  CHECK(at(c, src, 1) == "again");
}

// Hard breaks survive wrapping, including the blank line from '\n\n'.
TEST(hard_breaks_are_kept_and_blank_lines_occupy_a_row) {
  const std::string src = "a\n\nb";
  auto c = layout(src, 40.f);
  CHECK(c.line_count() == 3);
  CHECK(at(c, src, 0) == "a");
  CHECK(at(c, src, 1) == "");
  CHECK(at(c, src, 2) == "b");
  CHECK_APPROX(c.total_height(), LINE_H * 3.f);
}

// Zero wrap width means "do not soft wrap", NOT "one single line" -- hard
// breaks still break, which is what an unwrapped multi-line field shows.
TEST(zero_wrap_width_still_honours_hard_breaks) {
  const std::string src = "aaaaaaaaaa bbbbbbbbbb\nc";
  ti::TextLayoutCache c;
  c.rebuild(src, 0.f, LINE_H, measure);
  CHECK(c.line_count() == 2);
  CHECK(at(c, src, 0) == "aaaaaaaaaa bbbbbbbbbb");
  CHECK(at(c, src, 1) == "c");
}

TEST(empty_text_is_still_one_line) {
  auto c = layout("", 20.f);
  CHECK(c.line_count() == 1);
  CHECK(c.line(0).length == 0);
  CHECK_APPROX(c.total_height(), LINE_H);
}

// ---------------------------------------------------------------------------
// Source offsets -- the part the renderer never needs and the editor cannot
// work without.
// ---------------------------------------------------------------------------

// A soft break eats the space, so line 1 starts one PAST the end of line 0.
// Getting this wrong puts the caret a character off on every wrapped line.
TEST(soft_wrap_offsets_skip_the_consumed_space) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  CHECK(c.line(0).source_offset == 0);
  CHECK(c.line(0).length == 11);
  CHECK(c.line(1).source_offset == 12); // 11 + the consumed ' '
  CHECK(src.substr(12) == "again");
}

// The same for a hard break: the '\n' is not part of either line.
TEST(hard_break_offsets_skip_the_newline) {
  const std::string src = "ab\ncd";
  auto c = layout(src, 40.f);
  CHECK(c.line(0).source_offset == 0);
  CHECK(c.line(0).length == 2);
  CHECK(c.line(1).source_offset == 3);
  CHECK(c.line(1).length == 2);
}

// Leading indentation is content, not a break, so it must survive with its
// offsets intact.
TEST(leading_whitespace_is_preserved_with_its_offsets) {
  const std::string src = "a\n    indented";
  auto c = layout(src, 40.f);
  CHECK(c.line_count() == 2);
  CHECK(at(c, src, 1) == "    indented");
  CHECK(c.line(1).source_offset == 2);
}

// ---------------------------------------------------------------------------
// offset -> (row, column)
// ---------------------------------------------------------------------------

TEST(line_at_offset_finds_the_containing_line) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  CHECK(c.line_at_offset(0) == 0);
  CHECK(c.line_at_offset(5) == 0);
  CHECK(c.line_at_offset(12) == 1);
  CHECK(c.line_at_offset(17) == 1);
}

// The old implementation used a half-open containment test, so an offset
// sitting exactly at a line end matched NO line and fell through to the last
// one. That is where the caret sits every time you press End or type to the
// wrap point, so it would have jumped to the bottom of the field.
TEST(an_offset_at_a_line_end_stays_on_that_line) {
  const std::string src = "aa bb cc dd";
  auto c = layout(src, 5.f); // "aa bb" | "cc dd"
  CHECK(c.line_count() == 2);
  CHECK(c.line_at_offset(5) == 0); // end of line 0, not line 1 and not last
  CHECK(c.column_at_offset(5) == 5);
}

TEST(column_at_offset_is_relative_to_the_line) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  CHECK(c.column_at_offset(3) == 3);
  CHECK(c.column_at_offset(12) == 0); // first byte of line 1
  CHECK(c.column_at_offset(14) == 2);
}

// A cursor at the very end of the text belongs to the last line.
TEST(offset_at_end_of_text_is_on_the_last_line) {
  const std::string src = "ab\ncd";
  auto c = layout(src, 40.f);
  CHECK(c.line_at_offset(src.size()) == 1);
  CHECK(c.column_at_offset(src.size()) == 2);
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

TEST(line_at_y_maps_pixels_to_rows_and_clamps) {
  const std::string src = "a\nb\nc";
  auto c = layout(src, 40.f);
  CHECK(c.line_at_y(0.f, LINE_H) == 0);
  CHECK(c.line_at_y(LINE_H + 1.f, LINE_H) == 1);
  CHECK(c.line_at_y(-50.f, LINE_H) == 0);    // above clamps to the first
  CHECK(c.line_at_y(9999.f, LINE_H) == 2);   // below clamps to the last
}

TEST(y_position_advances_by_line_height) {
  const std::string src = "a\nb\nc";
  auto c = layout(src, 40.f);
  CHECK_APPROX(c.line(0).y_position, 0.f);
  CHECK_APPROX(c.line(1).y_position, LINE_H);
  CHECK_APPROX(c.line(2).y_position, LINE_H * 2.f);
  CHECK_APPROX(c.y_for_offset(4), LINE_H * 2.f);
}

// max_width drives auto-grow and the horizontal extent, so it has to be the
// widest LINE, not the width of the whole text.
TEST(max_width_is_the_widest_line) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  CHECK_APPROX(c.max_width(), 11.f * CHAR_W);
}

// ===========================================================================
// The widget itself
//
// Without a FontManager the widget measures at size * 0.5 per byte, which is
// the same stub the cases above use -- so these predict row counts rather than
// reading them back.
// ===========================================================================

namespace {
constexpr float WIDGET_FONT = 20.f; // -> 10px per byte, as above

// text_area lays out from the PREVIOUS frame's wrap, so a single frame never
// exercises auto-grow. Run two, like a real app does.
void two_frames(ImmTestHarness &h, const std::function<void()> &emit) {
  for (int i = 0; i < 2; i++) {
    h.begin_frame();
    emit();
    h.layout_only();
  }
}

int count_line_divs() {
  int n = 0;
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "text_area_line")
      n++;
  }
  return n;
}

ComponentConfig area_config(float w, float h) {
  return ComponentConfig{}
      .with_size(ComponentSize{pixels(w), pixels(h)})
      .with_font(UIComponent::DEFAULT_FONT, WIDGET_FONT)
      .with_line_height(pixels(LINE_H));
}
} // namespace

// The gap in one test: text_area stored word_wrap and never read it, so a long
// message stayed on one row and ran off the right edge.
TEST(a_long_message_wraps_onto_several_rows) {
  ImmTestHarness h;
  std::string text = "the quick brown fox jumps over the lazy dog";
  two_frames(h, [&] {
    text_area(h.context(), mk(h.root(), 0), text, area_config(200.f, 200.f));
  });
  CHECK(count_line_divs() > 1);
}

// Pins that the test above measures wrapping and not the field being small:
// same text, same box, wrapping off -> a single row.
TEST(word_wrap_off_leaves_one_row_per_source_line) {
  ImmTestHarness h;
  std::string text = "the quick brown fox jumps over the lazy dog";
  two_frames(h, [&] {
    text_area(h.context(), mk(h.root(), 0), text,
              area_config(200.f, 200.f).with_word_wrap(false));
  });
  CHECK(count_line_divs() == 1);
}

// ---------------------------------------------------------------------------
// Auto-grow
// ---------------------------------------------------------------------------

namespace {
float grown_height(const std::string &content, size_t max_lines) {
  ImmTestHarness h;
  std::string text = content;
  UIComponent *field = nullptr;
  two_frames(h, [&] {
    text_area(h.context(), mk(h.root(), 0), text,
              area_config(200.f, LINE_H)
                  .with_auto_grow()
                  .with_max_lines(max_lines));
  });
  field = h.find("text_area_field");
  return field ? field->rect().height : -1.f;
}
} // namespace

TEST(auto_grow_height_follows_the_row_count) {
  const float one = grown_height("short", 0);
  const float three = grown_height("a\nb\nc", 0);
  ui_test::check(one > 0.f && three > 0.f, "both laid out", __FILE__, __LINE__);
  // Two extra rows of content is two extra rows of height, padding unchanged.
  CHECK_APPROX(three - one, LINE_H * 2.f);
}

// A grown field is exactly tall enough for its content, so it must not also be
// scrolled. It was: the viewport came from the PREVIOUS frame's height, which
// is a row behind while growing, so the field scrolled to reach a caret that
// was already on screen and showed only the last row.
TEST(a_grown_field_is_not_also_scrolled) {
  ImmTestHarness h;
  std::string text = "a\nb\nc";
  Entity *area = nullptr;
  two_frames(h, [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, LINE_H).with_auto_grow());
    area = &r.ent();
  });
  ui_test::check(area != nullptr && area->has<ti::HasTextAreaState>(),
                 "state exists", __FILE__, __LINE__);
  if (area && area->has<ti::HasTextAreaState>())
    CHECK_APPROX(area->get<ti::HasTextAreaState>().scroll_offset_y, 0.f);
}

// Unbounded growth would push a composer over the whole window, so max_lines
// caps it and the field scrolls past that.
TEST(auto_grow_stops_at_max_lines) {
  const float capped = grown_height("a\nb\nc\nd\ne\nf\ng\nh", 3);
  const float three = grown_height("a\nb\nc", 3);
  CHECK_APPROX(capped, three);
}

// ---------------------------------------------------------------------------
// Enter vs Shift+Enter
// ---------------------------------------------------------------------------

namespace {
// Focus the field, then press Enter with shift optionally held.
std::string after_enter(bool submit_on_enter, bool shift, bool *submitted) {
  ImmTestHarness h;
  std::string text = "ab";
  auto emit = [&] {
    auto cfg = area_config(200.f, 100.f);
    if (submit_on_enter)
      cfg.with_submit_on_enter();
    auto r = text_area(h.context(), mk(h.root(), 0), text, cfg);
    r.ent().template addComponentIfMissing<ti::HasTextInputListener>();
    r.ent().template get<ti::HasTextInputListener>().on_submit =
        [submitted](Entity &) { *submitted = true; };
  };

  h.begin_frame();
  emit();
  h.layout_only();
  if (UIComponent *f = h.find("text_area_field"))
    h.context().focus_id = f->id;

  testing::input_injector::reset_all();
  if (shift)
    testing::input_injector::set_key_held(keys::LEFT_SHIFT);
  // pressed() reads last_action, not the held-actions bitset.
  h.context().last_action = ui_test::TestInputAction::WidgetPress;

  h.begin_frame();
  emit();
  h.layout_only();
  testing::input_injector::reset_all();
  return text;
}
} // namespace

// The default is unchanged: a text area is a text area, Enter breaks the line.
TEST(enter_breaks_the_line_by_default) {
  bool submitted = false;
  const std::string out = after_enter(false, false, &submitted);
  CHECK(out.find('\n') != std::string::npos);
  CHECK(!submitted);
}

// Chat-composer mode: Enter sends and must NOT leave a newline behind, or the
// draft the app reads back has a stray break in it.
TEST(submit_on_enter_sends_without_breaking) {
  bool submitted = false;
  const std::string out = after_enter(true, false, &submitted);
  CHECK(submitted);
  CHECK(out.find('\n') == std::string::npos);
}

// ...and Shift+Enter still breaks, which is the whole point of the pairing.
TEST(shift_enter_still_breaks_when_submitting_on_enter) {
  bool submitted = false;
  const std::string out = after_enter(true, true, &submitted);
  CHECK(out.find('\n') != std::string::npos);
  CHECK(!submitted);
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------

// Rows past the bottom of a fixed field must not paint over what sits below.
TEST(the_field_clips_its_rows) {
  ImmTestHarness h;
  std::string text = "a\nb\nc\nd\ne\nf\ng\nh";
  two_frames(h, [&] {
    text_area(h.context(), mk(h.root(), 0), text, area_config(200.f, 40.f));
  });
  Entity *field = nullptr;
  for (const auto &e : UICollectionHolder::get().collection.get_entities()) {
    if (e && e->has<UIComponentDebug>() &&
        e->get<UIComponentDebug>().name() == "text_area_field")
      field = e.get();
  }
  ui_test::check(field != nullptr, "the field exists", __FILE__, __LINE__);
  if (field)
    CHECK(field->has<HasClipChildren>());
}

// ---------------------------------------------------------------------------
// Keyboard controls
//
// Driven through the widget, not the helpers, because the wiring is the part
// that was missing -- move_cursor_word_left and friends already existed and
// text_area simply never called any of them.
// ---------------------------------------------------------------------------

namespace {
// Focus a text_area holding `start`, fire `action`, return the resulting state.
// Two emits: the widget needs a laid-out frame before its wrap is real.
ti::HasTextAreaState *press(ImmTestHarness &h, std::string &text,
                            ui_test::TestInputAction action,
                            size_t cursor_at, bool wrap = true) {
  Entity *area = nullptr;
  auto emit = [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, 200.f).with_word_wrap(wrap));
    area = &r.ent();
  };
  h.begin_frame();
  emit();
  h.layout_only();
  if (UIComponent *f = h.find("text_area_field"))
    h.context().focus_id = f->id;
  if (area && area->has<ti::HasTextAreaState>())
    area->get<ti::HasTextAreaState>().cursor_position = cursor_at;

  h.context().last_action = action;
  h.begin_frame();
  emit();
  h.layout_only();
  return area && area->has<ti::HasTextAreaState>()
             ? &area->get<ti::HasTextAreaState>()
             : nullptr;
}
} // namespace

TEST(alt_left_moves_a_word_back) {
  ImmTestHarness h;
  std::string text = "alpha beta gamma";
  auto *s = press(h, text, ui_test::TestInputAction::TextWordLeft, 16);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s)
    CHECK(s->cursor_position == 11); // start of "gamma"
}

TEST(alt_right_moves_a_word_forward) {
  ImmTestHarness h;
  std::string text = "alpha beta gamma";
  auto *s = press(h, text, ui_test::TestInputAction::TextWordRight, 0);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s)
    CHECK(s->cursor_position == 5); // end of "alpha"
}

TEST(alt_backspace_deletes_the_word_behind) {
  ImmTestHarness h;
  std::string text = "alpha beta gamma";
  press(h, text, ui_test::TestInputAction::TextDeleteWordBack, 16);
  CHECK(text == "alpha beta ");
}

TEST(alt_delete_deletes_the_word_ahead) {
  ImmTestHarness h;
  std::string text = "alpha beta gamma";
  press(h, text, ui_test::TestInputAction::TextDeleteWordForward, 0);
  CHECK(text == " beta gamma");
}

// Trailing whitespace goes with the word attached to it, in one press --
// deleting only the run of spaces and leaving the word behind is the wrong
// half. Same rule forwards, over leading whitespace.
TEST(alt_backspace_takes_trailing_spaces_and_the_word_together) {
  {
    ImmTestHarness h;
    std::string text = "alpha beta   ";
    press(h, text, ui_test::TestInputAction::TextDeleteWordBack, 13);
    CHECK(text == "alpha ");
  }
  {
    // A single trailing space is the same case, not a special one.
    ImmTestHarness h;
    std::string text = "alpha beta ";
    press(h, text, ui_test::TestInputAction::TextDeleteWordBack, 11);
    CHECK(text == "alpha ");
  }
  {
    ImmTestHarness h;
    std::string text = "   alpha beta";
    press(h, text, ui_test::TestInputAction::TextDeleteWordForward, 0);
    CHECK(text == " beta");
  }
}

// The cursor lands where the deletion started, not adrift in what is left.
TEST(alt_backspace_leaves_the_cursor_at_the_join) {
  ImmTestHarness h;
  std::string text = "alpha beta   ";
  auto *s = press(h, text, ui_test::TestInputAction::TextDeleteWordBack, 13);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s)
    CHECK(s->cursor_position == 6);
}

// Word motion skips the same span it would delete, so Alt+Left then
// Alt+Backspace do not disagree about where the word begins.
TEST(alt_left_skips_trailing_spaces_to_the_word_start) {
  ImmTestHarness h;
  std::string text = "alpha beta   ";
  auto *s = press(h, text, ui_test::TestInputAction::TextWordLeft, 13);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s)
    CHECK(s->cursor_position == 6);
}

// Home/End go to the ends of the VISUAL row. With wrapping on, the source-line
// versions would jump to the far end of the whole paragraph instead.
TEST(home_goes_to_the_start_of_the_wrapped_row) {
  ImmTestHarness h;
  // 200px wide at 10px/char, minus padding -> wraps well before the end.
  std::string text = "aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll";
  auto *s = press(h, text, ui_test::TestInputAction::TextHome, 40);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s) {
    const size_t row = s->layout_cache.line_at_offset(40);
    ui_test::check(row > 0, "offset 40 is on a wrapped row, not the first",
                   __FILE__, __LINE__);
    CHECK(s->cursor_position == s->layout_cache.line(row).source_offset);
    CHECK(s->cursor_position != 0); // NOT the start of the paragraph
  }
}

TEST(end_goes_to_the_end_of_the_wrapped_row) {
  ImmTestHarness h;
  std::string text = "aaa bbb ccc ddd eee fff ggg hhh iii jjj kkk lll";
  auto *s = press(h, text, ui_test::TestInputAction::TextEnd, 4);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s) {
    CHECK(s->cursor_position == s->layout_cache.line(0).end_offset());
    CHECK(s->cursor_position != text.size()); // NOT the end of the paragraph
  }
}

// ---------------------------------------------------------------------------
// Wheel scrolling
// ---------------------------------------------------------------------------

namespace {
// Put the mouse over the field, turn the wheel, and report the scroll offset.
float scroll_after_wheel(const std::string &content, float box_h, float wheel) {
  ImmTestHarness h;
  std::string text = content;
  Entity *area = nullptr;
  auto emit = [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, box_h));
    area = &r.ent();
  };
  h.begin_frame();
  emit();
  h.layout_only();
  // Park the cursor at the top so ensure_cursor_visible is not what moves it.
  if (area && area->has<ti::HasTextAreaState>())
    area->get<ti::HasTextAreaState>().cursor_position = 0;

  if (UIComponent *f = h.find("text_area_field")) {
    const RectangleType r = f->rect();
    h.context().mouse.pos =
        Vector2Type{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
  }
  // get_mouse_wheel_move_v only reads the injector in test mode.
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::input_injector::set_mouse_wheel(0.f, wheel);

  h.begin_frame();
  emit();
  h.layout_only();
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
  return area && area->has<ti::HasTextAreaState>()
             ? area->get<ti::HasTextAreaState>().scroll_offset_y
             : -1.f;
}
} // namespace

TEST(the_wheel_scrolls_a_field_whose_content_overflows) {
  // Eight rows in a three-row box.
  const float down = scroll_after_wheel("a\nb\nc\nd\ne\nf\ng\nh", 70.f, -1.f);
  CHECK_APPROX(down, LINE_H);
}

TEST(the_wheel_does_not_scroll_past_the_top) {
  const float up = scroll_after_wheel("a\nb\nc\nd\ne\nf\ng\nh", 70.f, 1.f);
  CHECK_APPROX(up, 0.f);
}

// The wheel is consume-once, so a field that cannot scroll must leave it for
// whatever scroll view encloses it rather than swallowing it.
TEST(a_field_that_fits_its_content_leaves_the_wheel_alone) {
  const float none = scroll_after_wheel("a\nb", 200.f, -1.f);
  CHECK_APPROX(none, 0.f);
}

// ---------------------------------------------------------------------------
// Selection geometry
//
// These work in SOURCE offsets, not joined_text offsets. A soft break consumes
// the space it broke at, so the two spaces disagree by one byte per wrapped
// row -- which is exactly the drift that would put a highlight a character off
// on every line after the first.
// ---------------------------------------------------------------------------

TEST(a_range_inside_one_row_is_one_rect) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f); // "hello world" | "again"
  auto rects = ti::selection_rects(c, src, 0, 5, Vector2Type{0.f, 0.f}, LINE_H,
                                   measure);
  CHECK(rects.size() == 1);
  if (rects.size() == 1) {
    CHECK_APPROX(rects[0].x, 0.f);
    CHECK_APPROX(rects[0].width, 5.f * CHAR_W);
    CHECK_APPROX(rects[0].height, LINE_H);
  }
}

// A range spanning rows yields one rect per row, first and last partial.
TEST(a_range_across_rows_is_one_rect_per_row) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  auto rects = ti::selection_rects(c, src, 6, 14, Vector2Type{0.f, 0.f},
                                   LINE_H, measure);
  CHECK(rects.size() == 2);
  if (rects.size() == 2) {
    // Row 0: "world" -- from byte 6 to the end of the row.
    CHECK_APPROX(rects[0].x, 6.f * CHAR_W);
    CHECK_APPROX(rects[0].width, 5.f * CHAR_W);
    CHECK_APPROX(rects[0].y, 0.f);
    // Row 1: "ag" -- source offset 12, so the range covers its first 2 bytes.
    CHECK_APPROX(rects[1].x, 0.f);
    CHECK_APPROX(rects[1].width, 2.f * CHAR_W);
    CHECK_APPROX(rects[1].y, LINE_H);
  }
}

TEST(an_empty_range_yields_no_rects) {
  const std::string src = "hello world";
  auto c = layout(src, 20.f);
  CHECK(ti::selection_rects(c, src, 4, 4, Vector2Type{0.f, 0.f}, LINE_H,
                            measure)
            .empty());
}

// The break between rows is not drawn, so selecting only across it paints
// nothing rather than a stray sliver.
TEST(a_range_covering_only_a_break_paints_nothing) {
  const std::string src = "ab\ncd";
  auto c = layout(src, 40.f);
  auto rects = ti::selection_rects(c, src, 2, 3, Vector2Type{0.f, 0.f},
                                   LINE_H, measure);
  CHECK(rects.empty());
}

TEST(origin_offsets_every_rect) {
  const std::string src = "abcd";
  auto c = layout(src, 40.f);
  auto rects = ti::selection_rects(c, src, 0, 2, Vector2Type{7.f, 3.f},
                                   LINE_H, measure);
  CHECK(rects.size() == 1);
  if (rects.size() == 1) {
    CHECK_APPROX(rects[0].x, 7.f);
    CHECK_APPROX(rects[0].y, 3.f);
  }
}

// ---------------------------------------------------------------------------
// point -> offset
// ---------------------------------------------------------------------------

TEST(offset_at_point_finds_the_byte_under_the_cursor) {
  const std::string src = "hello world again";
  auto c = layout(src, 11.f);
  // Row 0, three characters in.
  CHECK(ti::offset_at_point(c, src, Vector2Type{3.f * CHAR_W, 5.f}, LINE_H,
                            measure) == 3);
  // Row 1, two characters in -- source offset 12 + 2, NOT 11 + 2. This is the
  // consumed-space byte that a joined_text mapping would lose.
  CHECK(ti::offset_at_point(c, src, Vector2Type{2.f * CHAR_W, LINE_H + 5.f},
                            LINE_H, measure) == 14);
}

TEST(offset_at_point_clamps_off_the_ends) {
  const std::string src = "ab\ncd";
  auto c = layout(src, 40.f);
  // Above the first row, and left of it.
  CHECK(ti::offset_at_point(c, src, Vector2Type{-99.f, -99.f}, LINE_H,
                            measure) == 0);
  // Below the last row and off its right edge clamps to the end of the text.
  CHECK(ti::offset_at_point(c, src, Vector2Type{999.f, 999.f}, LINE_H,
                            measure) == 5);
}

// ---------------------------------------------------------------------------
// Selection through the widget
// ---------------------------------------------------------------------------

TEST(select_all_selects_the_whole_text) {
  ImmTestHarness h;
  std::string text = "alpha beta";
  auto *s = press(h, text, ui_test::TestInputAction::TextSelectAll, 0);
  ui_test::check(s != nullptr, "state exists", __FILE__, __LINE__);
  if (s) {
    CHECK(s->has_selection());
    CHECK(s->selection_start() == 0);
    CHECK(s->selection_end() == text.size());
    CHECK(s->selected_text() == "alpha beta");
  }
}

// Typing over a selection replaces it -- without this, select-all then type
// appends instead of overwriting.
TEST(typing_replaces_the_selection) {
  ImmTestHarness h;
  std::string text = "alpha beta";
  Entity *area = nullptr;
  auto emit = [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, 200.f));
    area = &r.ent();
  };
  h.begin_frame();
  emit();
  h.layout_only();
  if (UIComponent *f = h.find("text_area_field"))
    h.context().focus_id = f->id;
  if (area && area->has<ti::HasTextAreaState>()) {
    auto &s = area->get<ti::HasTextAreaState>();
    s.selection_anchor = 0;
    s.cursor_position = 5; // "alpha" selected
  }
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::test_input::push_char('X');
  h.begin_frame();
  emit();
  h.layout_only();
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
  CHECK(text == "X beta");
}

// Backspace with a selection deletes the selection, not one character behind
// the cursor.
TEST(backspace_deletes_the_selection) {
  ImmTestHarness h;
  std::string text = "alpha beta";
  Entity *area = nullptr;
  auto emit = [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, 200.f));
    area = &r.ent();
  };
  h.begin_frame();
  emit();
  h.layout_only();
  if (UIComponent *f = h.find("text_area_field"))
    h.context().focus_id = f->id;
  if (area && area->has<ti::HasTextAreaState>()) {
    auto &s = area->get<ti::HasTextAreaState>();
    s.selection_anchor = 0;
    s.cursor_position = 6;
  }
  h.context().last_action = ui_test::TestInputAction::TextBackspace;
  h.begin_frame();
  emit();
  h.layout_only();
  CHECK(text == "beta");
}

// Shift+Left extends rather than collapsing, which is what makes keyboard
// selection possible at all.
TEST(shift_arrow_extends_the_selection) {
  ImmTestHarness h;
  std::string text = "alpha beta";
  Entity *area = nullptr;
  auto emit = [&] {
    auto r = text_area(h.context(), mk(h.root(), 0), text,
                       area_config(200.f, 200.f));
    area = &r.ent();
  };
  h.begin_frame();
  emit();
  h.layout_only();
  if (UIComponent *f = h.find("text_area_field"))
    h.context().focus_id = f->id;
  if (area && area->has<ti::HasTextAreaState>())
    area->get<ti::HasTextAreaState>().cursor_position = 10;

  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::input_injector::set_key_held(keys::LEFT_SHIFT);
  h.context().last_action = ui_test::TestInputAction::WidgetLeft;
  h.begin_frame();
  emit();
  h.layout_only();
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;

  if (area && area->has<ti::HasTextAreaState>()) {
    auto &s = area->get<ti::HasTextAreaState>();
    CHECK(s.has_selection());
    CHECK(s.selected_text() == "a");
  }
}

int main() { return ui_test::run_registered_tests("text_area"); }
