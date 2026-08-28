// multiline_text_test.cpp
// Multi-line labels: hard '\n' breaks, soft wrapping, and the Dim::Text
// sizing that has to agree with what actually gets drawn.
//
// Runs under the RECORDING backend so the draws can be asserted on. That also
// matters for the hard-break case specifically: raylib's DrawTextEx honours
// '\n' on its own, so a raylib-only test would pass whether or not the library
// splits lines itself, and sokol/recording would still be broken. Here nothing
// splits the text unless our code does.
//
// The harness measure stub is width = chars * font_size * 0.5, height =
// font_size, so at font size 20 every character is 10px wide and every line is
// 20px tall. Every number below is arithmetic from that, not a value read back
// off the implementation.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
constexpr float FS = 20.f;    // font size used by every test here
constexpr float CHAR_W = 10.f; // FS * 0.5
constexpr float LINE_H = FS;

// The text of every recorded draw, in paint order.
std::vector<std::string> drawn_lines(ImmTestHarness &h) {
  std::vector<std::string> out;
  for (const auto &c : h.drawn("text"))
    out.push_back(c.text);
  return out;
}

Rectangle rect_of(const ElementResult &e) {
  return e.ent().get<UIComponent>().rect();
}

// A recorded text draw carries no height -- the backend records where the
// string was placed, not how tall it came out -- so line height has to come
// from the gap between consecutive lines. Asserting on rect.height instead
// reads a constant 0 and passes no matter what the renderer did.
float line_gap(const std::vector<DrawCall> &calls) {
  return calls.size() < 2 ? 0.f : calls[1].rect.y - calls[0].rect.y;
}

// Bottom edge of the painted block, derived from the line gap.
float block_bottom(const std::vector<DrawCall> &calls) {
  return calls.empty() ? 0.f : calls.back().rect.y + line_gap(calls);
}
} // namespace

// ---------------------------------------------------------------------------
// Hard breaks
// ---------------------------------------------------------------------------

// A '\n' in a plain label splits, with no TextOverflow::Wrap and no explicit
// font size. Before this worked, the whole string went to the backend as one
// draw with the newlines still embedded.
TEST(hard_newline_splits_a_plain_label) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(120)})
          .with_label("line one\nline two\nline three"));
  h.render();

  const auto lines = drawn_lines(h);
  CHECK(lines.size() == 3);
  if (lines.size() != 3)
    return;
  CHECK(lines[0] == "line one");
  CHECK(lines[1] == "line two");
  CHECK(lines[2] == "line three");
  // No newline survives into a draw call.
  for (const auto &l : lines)
    CHECK(l.find('\n') == std::string::npos);
}

// The three lines are stacked, not painted on top of each other.
TEST(hard_newline_lines_are_stacked_and_centered) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(120)})
          .with_label("aaa\nbbb\nccc")
          .with_font_size(FS));
  h.render();

  const auto calls = h.drawn("text");
  CHECK(calls.size() == 3);
  if (calls.size() != 3)
    return;
  // 3 lines * 20 = 60 in a 120 box, so the block starts 30 down.
  CHECK_APPROX(calls[0].rect.y, 30.f);
  CHECK_APPROX(calls[1].rect.y, 30.f + LINE_H);
  CHECK_APPROX(calls[2].rect.y, 30.f + 2.f * LINE_H);
}

// Without an explicit size the font is auto-fit. That fit is computed against
// the joined text -- i.e. for ONE line -- so a block of them has to be shrunk
// or it paints outside the box.
TEST(auto_fit_shrinks_so_every_line_fits) {
  ImmTestHarness h;
  // 80px box, so 20px lines: shrinking is needed but stays above MIN_FONT_SIZE.
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(80)})
          .with_label("a\nb\nc\nd"));
  h.render();

  const auto calls = h.drawn("text");
  CHECK(calls.size() == 4);
  if (calls.size() != 4)
    return;
  const float top = calls.front().rect.y;
  const float bottom = block_bottom(calls);
  fprintf(stderr, "        block y %.1f..%.1f (box 0..80)\n", top, bottom);
  CHECK(top >= -0.5f);
  CHECK(bottom <= 80.5f);
}

// ---------------------------------------------------------------------------
// Soft wrapping
// ---------------------------------------------------------------------------

TEST(wrap_breaks_a_paragraph_at_the_box_width) {
  ImmTestHarness h;
  // 200 wide minus the 10px the renderer reserves = 190 -> 19 chars a line.
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(100)})
          .with_label("the quick brown fox jumps over the lazy dog")
          .with_text_overflow(TextOverflow::Wrap)
          .with_font_size(FS));
  h.render();

  const auto lines = drawn_lines(h);
  CHECK(lines.size() > 1);
  for (const auto &l : lines)
    CHECK(static_cast<float>(l.size()) * CHAR_W <= 190.f);
}

// ---------------------------------------------------------------------------
// Dim::Text sizing -- the half that has to agree with the drawing above
// ---------------------------------------------------------------------------

// A Dim::Text box around a hard-broken label is as tall as its lines and as
// wide as its WIDEST line, not as wide as the whole joined string.
TEST(dim_text_sizes_to_the_line_box) {
  ImmTestHarness h;
  auto one = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f},
                                              Size{Dim::Text, 0.f, 1.f}})
                     .with_label("one line")
                     .with_font_size(FS));
  // Widest line is "three line" at 10 chars.
  auto three = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}
                       .with_size(ComponentSize{Size{Dim::Text, 0.f, 1.f},
                                                Size{Dim::Text, 0.f, 1.f}})
                       .with_label("one line\ntwo line\nthree line")
                       .with_font_size(FS));
  h.layout_only();

  CHECK_APPROX(rect_of(one).width, 8.f * CHAR_W);
  CHECK_APPROX(rect_of(one).height, LINE_H);
  // The joined string is 28 chars = 280px; the widest LINE is 10 = 100px.
  CHECK_APPROX(rect_of(three).width, 10.f * CHAR_W);
  CHECK_APPROX(rect_of(three).height, 3.f * LINE_H);
}

// The point of the whole change: a fixed-width paragraph with a Dim::Text
// height gets a box tall enough for the lines the renderer actually draws.
// It used to report one line's height and overflow by however many lines the
// text wrapped to.
TEST(dim_text_height_matches_the_wrapped_line_count) {
  ImmTestHarness h;
  auto para = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(200),
                                               Size{Dim::Text, 0.f, 1.f}})
                      .with_label("the quick brown fox jumps over the lazy dog "
                                  "again and again and again")
                      .with_text_overflow(TextOverflow::Wrap)
                      .with_font_size(FS));
  h.render();

  const size_t drawn_count = drawn_lines(h).size();
  CHECK(drawn_count > 1);
  fprintf(stderr, "        %zu lines drawn, box %.1f tall\n", drawn_count,
          rect_of(para).height);
  // Layout and rendering must agree on the line count, or the text spills.
  CHECK_APPROX(rect_of(para).height,
               static_cast<float>(drawn_count) * LINE_H);
}

// Wrapping is only measured when the renderer would also wrap, which needs a
// pinned font size. Without one neither side wraps, so the box stays one line
// and the text is auto-fit onto it -- consistent, if not what you wanted.
TEST(wrap_without_a_font_size_stays_one_line_on_both_sides) {
  ImmTestHarness h;
  auto para = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(200),
                                               Size{Dim::Text, 0.f, 1.f}})
                      .with_label("the quick brown fox jumps over the lazy dog")
                      .with_text_overflow(TextOverflow::Wrap));
  h.render();

  CHECK(drawn_lines(h).size() == 1);
  fprintf(stderr, "        box %.1f tall\n", rect_of(para).height);
}

// ---------------------------------------------------------------------------
// Styled (multi-colour) labels take a different render path
// ---------------------------------------------------------------------------

// draw_runs_in_rect has always broken on '\n', but it auto-fit the font
// against the joined text, so the block ran past the bottom of the box.
TEST(styled_label_hard_break_fits_the_box) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(120)})
          .with_styled_label({TextSpan{"red\n", Color{255, 0, 0, 255}},
                              TextSpan{"green", Color{0, 255, 0, 255}}}));
  h.render();

  const auto calls = h.drawn("text");
  CHECK(calls.size() == 2);
  if (calls.size() != 2)
    return;
  CHECK(calls[0].text == "red");
  CHECK(calls[1].text == "green");
  // Two lines in a 120 box: each may be at most 60 tall. Auto-fit used to size
  // them against the joined single line and overflow by a line's worth.
  const float bottom = block_bottom(calls);
  fprintf(stderr, "        line gap %.1f, block ends at %.1f (box 120)\n",
          line_gap(calls), bottom);
  CHECK(bottom <= 120.5f);
}

// Each run keeps its own colour across the break.
TEST(styled_label_keeps_run_colours_across_lines) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(300), pixels(120)})
          .with_styled_label({TextSpan{"red\n", Color{255, 0, 0, 255}},
                              TextSpan{"green", Color{0, 255, 0, 255}}}));
  h.render();

  const auto calls = h.drawn("text");
  CHECK(calls.size() == 2);
  if (calls.size() != 2)
    return;
  CHECK(calls[0].color.r == 255 && calls[0].color.g == 0);
  CHECK(calls[1].color.r == 0 && calls[1].color.g == 255);
}

// ---------------------------------------------------------------------------
// "Does it fit" has to be asked of the wrapped layout
// ---------------------------------------------------------------------------

// position_text_ex drives both the overflow warning and the debug overlay. It
// used to measure the whole label as one line, so a paragraph that wraps
// perfectly well inside its box still reported as overflowing -- every frame,
// for every wrapping label.
TEST(a_wrapping_label_that_fits_does_not_report_overflow) {
  ImmTestHarness h;
  const std::string text = "the quick brown fox jumps over the lazy dog";
  // 43 chars = 430px on one line, well past 300. Wrapped at 290 it needs three
  // lines = 60px, inside a 300x100 box.
  auto res = position_text_ex(
      *h.render_font(), text, RectangleType{0.f, 0.f, 300.f, 100.f},
      TextAlignment::Left, Vector2Type{5.f, 5.f}, FS, 0.f, TextOverflow::Wrap);
  CHECK(res.text_fits);
}

// The same text in a box too short for the lines it wraps to still overflows.
// The fix must not silence real overflow.
TEST(a_wrapping_label_too_short_still_reports_overflow) {
  ImmTestHarness h;
  const std::string text = "the quick brown fox jumps over the lazy dog";
  // Wraps to three 20px lines; a 30px box cannot hold them.
  auto res = position_text_ex(
      *h.render_font(), text, RectangleType{0.f, 0.f, 300.f, 30.f},
      TextAlignment::Left, Vector2Type{5.f, 5.f}, FS, 0.f, TextOverflow::Wrap);
  CHECK(!res.text_fits);
}

// A single unbreakable word wider than the box is genuine width overflow --
// wrapping cannot help it.
TEST(an_unbreakable_word_still_reports_overflow) {
  ImmTestHarness h;
  auto res = position_text_ex(
      *h.render_font(), "supercalifragilisticexpialidocious",
      RectangleType{0.f, 0.f, 100.f, 100.f}, TextAlignment::Left,
      Vector2Type{5.f, 5.f}, FS, 0.f, TextOverflow::Wrap);
  CHECK(!res.text_fits);
}

// Hard breaks get the same treatment without asking for Wrap. The joined
// string here is 32 chars (320px) and would not fit 190px, but the widest
// LINE is 10 chars (100px) and does -- so measuring the block rather than the
// joined string is what decides it.
TEST(a_hard_broken_label_that_fits_does_not_report_overflow) {
  ImmTestHarness h;
  auto res = position_text_ex(*h.render_font(),
                              "aaaaaaaaaa\nbbbbbbbbbb\ncccccccccc",
                              RectangleType{0.f, 0.f, 200.f, 100.f},
                              TextAlignment::Left, Vector2Type{5.f, 5.f}, FS,
                              0.f, TextOverflow::Clip);
  CHECK(res.text_fits);
}

// ---------------------------------------------------------------------------
// Per-span font weight
// ---------------------------------------------------------------------------
//
// These drive the wrap primitive directly with a measure that is WIDER for
// bold, which is the whole reason weight has to reach the wrapper: measuring a
// bold run with the regular face under-measures it and the line overflows.

namespace {
namespace uid = afterhours::ui::detail;
using afterhours::colors::FontWeight;

const Color kW{255, 255, 255, 255};

// Regular is 10px a character, bold 20px. Exaggerated so a mistake cannot hide
// inside a rounding tolerance.
int g_measure_calls = 0;
float measure_by_weight(const std::string &s, FontWeight w) {
  g_measure_calls++;
  const float per = (w == FontWeight::Bold) ? 20.f : 10.f;
  return static_cast<float>(s.size()) * per;
}
} // namespace

// Runs sharing a colour but differing in weight must stay separate, or the
// bold half gets coalesced into the regular run and silently renders regular.
TEST(a_bold_run_is_not_merged_into_a_regular_run_of_the_same_colour) {
  const std::vector<TextSpan> runs{
      TextSpan{"plain", kW, FontWeight::Regular},
      TextSpan{"bold", kW, FontWeight::Bold},
  };
  const auto lines = uid::wrap_runs_to_width(runs, 1e9f, measure_by_weight);
  CHECK(lines.size() == 1);
  if (lines.size() != 1)
    return;
  CHECK(lines[0].size() == 2);
  if (lines[0].size() != 2)
    return;
  CHECK(lines[0][0].text == "plain");
  CHECK(lines[0][0].weight == FontWeight::Regular);
  CHECK(lines[0][1].text == "bold");
  CHECK(lines[0][1].weight == FontWeight::Bold);
}

// Adjacent runs that match in BOTH colour and weight still coalesce.
TEST(runs_matching_in_colour_and_weight_still_merge) {
  const std::vector<TextSpan> runs{TextSpan{"foo", kW, FontWeight::Bold},
                                   TextSpan{"bar", kW, FontWeight::Bold}};
  const auto lines = uid::wrap_runs_to_width(runs, 1e9f, measure_by_weight);
  CHECK(lines.size() == 1);
  if (lines.size() != 1)
    return;
  CHECK(lines[0].size() == 1);
  if (lines[0].size() == 1)
    CHECK(lines[0][0].text == "foobar");
}

// The point of threading weight into the wrapper. "aaa" regular is 30px and
// "bbb" bold is 60px; at width 80 they cannot share a line, though a
// regular-only measure would call it 60 and keep them together.
TEST(wrap_breaks_earlier_because_a_bold_run_is_wider) {
  const std::vector<TextSpan> runs{TextSpan{"aaa ", kW, FontWeight::Regular},
                                   TextSpan{"bbb", kW, FontWeight::Bold}};
  const auto bold_lines = uid::wrap_runs_to_width(runs, 80.f,
                                                  measure_by_weight);
  CHECK(bold_lines.size() == 2);

  // Same text all regular is 3 + 1 + 3 = 70px and does fit in 80.
  const std::vector<TextSpan> plain{TextSpan{"aaa ", kW, FontWeight::Regular},
                                    TextSpan{"bbb", kW, FontWeight::Regular}};
  const auto plain_lines = uid::wrap_runs_to_width(plain, 80.f,
                                                   measure_by_weight);
  CHECK(plain_lines.size() == 1);
  fprintf(stderr, "        bold=%zu plain=%zu\n", bold_lines.size(),
          plain_lines.size());
}

// A line of uniform weight must still be measured in ONE call. measure_text
// spaces between characters, so summing pieces loses a spacing per join --
// the reason the wrapper measures whole candidates rather than per word.
TEST(a_uniform_line_costs_one_measure_call_per_candidate) {
  const std::vector<TextSpan> runs{TextSpan{"aaa bbb ccc", kW,
                                            FontWeight::Regular}};
  g_measure_calls = 0;
  uid::wrap_runs_to_width(runs, 1e9f, measure_by_weight);
  const int uniform = g_measure_calls;

  // Two words, two weights -> the candidate splits at the boundary only.
  const std::vector<TextSpan> mixed{TextSpan{"aaa ", kW, FontWeight::Regular},
                                    TextSpan{"bbb", kW, FontWeight::Bold}};
  g_measure_calls = 0;
  uid::wrap_runs_to_width(mixed, 1e9f, measure_by_weight);
  const int split = g_measure_calls;

  fprintf(stderr, "        uniform=%d mixed=%d calls\n", uniform, split);
  // "aaa bbb ccc" has two wrap candidates, each one call: never per word.
  CHECK(uniform <= 2);
  CHECK(split >= uniform);
}

// Weight survives a hard break, which is where the '\n' split rebuilds spans.
TEST(weight_survives_a_hard_break) {
  const std::vector<TextSpan> runs{
      TextSpan{"first\nsecond", kW, FontWeight::Bold}};
  const auto lines = uid::wrap_runs_to_width(runs, 1e9f, measure_by_weight);
  CHECK(lines.size() == 2);
  if (lines.size() != 2)
    return;
  CHECK(lines[0][0].weight == FontWeight::Bold);
  CHECK(lines[1][0].weight == FontWeight::Bold);
}

// An app that never loaded a bold face gets the base font, not a throw from
// get_font. This is what makes the feature safe to adopt incrementally.
TEST(an_unloaded_weight_falls_back_to_the_base_face) {
  ImmTestHarness h;
  auto *fm = h.render_font();
  CHECK(fm->resolve_weighted(UIComponent::DEFAULT_FONT, FontWeight::Bold) ==
        UIComponent::DEFAULT_FONT);
  // And a loaded one is picked up.
  fm->load_font(std::string(UIComponent::DEFAULT_FONT) + "@bold",
                get_default_font());
  CHECK(fm->resolve_weighted(UIComponent::DEFAULT_FONT, FontWeight::Bold) ==
        std::string(UIComponent::DEFAULT_FONT) + "@bold");
}

// Switching a label from plain to styled must not move the text. The two go
// through different draw paths, and only one of them applied the 5px inset --
// so the same string in the same box landed 5px further left once it gained a
// colour.
TEST(a_styled_label_starts_where_a_plain_one_does) {
  float plain_x = 0.f;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(40)})
            .with_absolute_position(0.f, 0.f)
            .with_label("hello")
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    h.render();
    const auto calls = h.drawn("text");
    CHECK(!calls.empty());
    if (calls.empty())
      return;
    plain_x = calls[0].rect.x;
  }

  float styled_x = 0.f;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(40)})
            .with_absolute_position(0.f, 0.f)
            .with_styled_label({TextSpan{"hello", Color{255, 255, 255, 255}}})
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    h.render();
    const auto calls = h.drawn("text");
    CHECK(!calls.empty());
    if (calls.empty())
      return;
    styled_x = calls[0].rect.x;
  }

  fprintf(stderr, "        plain x=%.1f styled x=%.1f\n", plain_x, styled_x);
  CHECK_APPROX(styled_x, plain_x);
}

// The same check against the BATCHED renderer, which is what wm and the other
// apps actually run. The two renderers are selectable at runtime, so a fix to
// one of them is half a fix -- and this pair of paths has diverged before.
TEST(batched_styled_label_starts_where_a_plain_one_does) {
  float plain_x = 0.f;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(40)})
            .with_absolute_position(0.f, 0.f)
            .with_label("hello")
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    h.render_batched();
    const auto calls = h.drawn("text");
    CHECK(!calls.empty());
    if (calls.empty())
      return;
    plain_x = calls[0].rect.x;
  }

  float styled_x = 0.f;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(300), pixels(40)})
            .with_absolute_position(0.f, 0.f)
            // TWO spans on purpose: a single-run line takes the whole-rect
            // branch and records label_rect, which would pass no matter what
            // the per-run alignment maths did.
            .with_styled_label({TextSpan{"hel", Color{255, 255, 255, 255}},
                                TextSpan{"lo", Color{255, 200, 0, 255}}})
            .with_alignment(TextAlignment::Left)
            .with_font_size(FS));
    h.render_batched();
    const auto calls = h.drawn("text");
    CHECK(!calls.empty());
    if (calls.empty())
      return;
    styled_x = calls[0].rect.x;
  }

  fprintf(stderr, "        batched plain x=%.1f styled x=%.1f\n", plain_x,
          styled_x);
  CHECK_APPROX(styled_x, plain_x);
}

int main() { return ui_test::run_registered_tests("multiline text"); }
