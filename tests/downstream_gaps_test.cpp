// downstream_gaps_test.cpp
// Executable repros for gaps that downstream apps reported against
// this library (see the "Downstream app feedback" section of todo.md).
//
// Why these live together rather than in the per-widget suites: each one
// starts life as a *claim* from an app's prose doc, and the first job is to
// find out whether the claim is true here and now. D5 is the cautionary tale
// -- the report named the wrong function AND the wrong call path, and only
// building the repro showed the real root cause was two levels down.
//
// Two rules for this file:
//
//   1. Nothing is commented out. A commented-out test looks like coverage and
//      runs nothing, which is how a build starts lying to you. A gap we have
//      not fixed yet gets a test that pins TODAY's behaviour with a comment
//      naming the behaviour we want, so the day someone implements the feature
//      this suite fails and makes them come update it.
//
//   2. Every test says which app reported it and which todo.md item it is, so
//      a green test can be traced back and the reporting app told to delete
//      its workaround.

#include "ui_test_harness.h"

#include <optional>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

namespace {
Rectangle rect_of(const ElementResult &e) {
  return e.ent().get<UIComponent>().rect();
}
} // namespace

// ===========================================================================
// D2 -- expand() in a Row, at the imm layer
//
// Reported by BOTH apps, which is why it is first.
//   app A #18:         "no flex-grow: can't pin a trailing element to the
//                       right edge"
//   app B:             "Row Flex Layout Broken with expand() Children" --
//                       "any child sized with expand() consumes the full
//                       parent width instead of the remaining width after
//                       fixed-size siblings"
//
// Both paid real cost: one hand-computes
// `labelW = rowContentW - leadSlot - countColW` across three row types;
// the other bakes whole rows into a single label string and gives up on
// coloured status letters.
//
// autolayout_test already covers this at the engine level and passes, both
// with NoWrap and with the Wrap default. These drive the imm `div()` API the
// apps actually call, which is the layer their reports describe.
// ===========================================================================

// The reported row shape: a fixed leading icon, an expanding label, a fixed
// trailing count. If expand() took the full width, the count would be pushed
// out of the row instead of sitting flush against its right edge.
TEST(d2_expand_row_pins_trailing_element) {
  ImmTestHarness h;
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(300), pixels(40)})
                     .with_flex_direction(FlexDirection::Row));
  auto icon = div(h.context(), mk(row.ent(), 0),
                  ComponentConfig{}.with_size(
                      ComponentSize{pixels(18), pixels(40)}));
  auto label = div(h.context(), mk(row.ent(), 1),
                   ComponentConfig{}.with_size(
                       ComponentSize{expand(), pixels(40)}));
  auto count = div(h.context(), mk(row.ent(), 2),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(24), pixels(40)}));
  h.layout_only();

  CHECK_APPROX(rect_of(icon).width, 18.f);
  // 300 - 18 - 24. The whole complaint is that this comes back as 300.
  CHECK_APPROX(rect_of(label).width, 258.f);
  CHECK_APPROX(rect_of(count).width, 24.f);

  // The point of the exercise: one shared right edge, no pixel bookkeeping.
  Rectangle r = rect_of(row), c = rect_of(count);
  CHECK_APPROX(c.x + c.width, r.x + r.width);
}

// The reporting app's actual case rather than the reduced one: the row is percent-sized
// because the usable width moves with sidebar and scrollbar state, which is
// exactly why it could not just hardcode percent(0.72f) for the label.
TEST(d2_expand_row_under_percent_parent) {
  ImmTestHarness h;
  auto panel = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(400), pixels(200)}));
  auto row = div(h.context(), mk(panel.ent(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{percent(1.0f), pixels(40)})
                     .with_flex_direction(FlexDirection::Row));
  auto icon = div(h.context(), mk(row.ent(), 0),
                  ComponentConfig{}.with_size(
                      ComponentSize{pixels(18), pixels(40)}));
  auto label = div(h.context(), mk(row.ent(), 1),
                   ComponentConfig{}.with_size(
                       ComponentSize{expand(), pixels(40)}));
  auto count = div(h.context(), mk(row.ent(), 2),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(24), pixels(40)}));
  h.layout_only();

  CHECK_APPROX(rect_of(label).width, 358.f); // 400 - 18 - 24

  Rectangle r = rect_of(row), c = rect_of(count);
  CHECK_APPROX(c.x + c.width, r.x + r.width);
}

// Two rows of different composition must land their trailing counts on the
// SAME right edge. This is the assertion the reporter could not make: its smart-view
// counts and folder counts sat ~17px apart, each internally consistent but not
// sharing an edge.
TEST(d2_expand_rows_share_one_right_edge) {
  ImmTestHarness h;
  auto panel = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(320), pixels(200)}));

  auto make_row = [&](int key, Size lead) {
    auto row = div(h.context(), mk(panel.ent(), key),
                   ComponentConfig{}
                       .with_size(ComponentSize{percent(1.0f), pixels(30)})
                       .with_flex_direction(FlexDirection::Row));
    div(h.context(), mk(row.ent(), 0),
        ComponentConfig{}.with_size(ComponentSize{lead, pixels(30)}));
    div(h.context(), mk(row.ent(), 1),
        ComponentConfig{}.with_size(ComponentSize{expand(), pixels(30)}));
    return div(h.context(), mk(row.ent(), 2),
               ComponentConfig{}.with_size(
                   ComponentSize{pixels(24), pixels(30)}));
  };

  // Different leading slots: one row has an icon, the other a wider chevron.
  auto count_a = make_row(0, pixels(18));
  auto count_b = make_row(1, pixels(40));
  h.layout_only();

  Rectangle a = rect_of(count_a), b = rect_of(count_b);
  CHECK_APPROX(a.x + a.width, b.x + b.width);
}

// floatinghotel's report says "a button OR div with FlexDirection::Row
// contains children". button() has its own label-sizing path, so it is worth
// its own case: the reported symptom there was the fixed status letter
// wrapping onto the line below the expanding filename.
TEST(d2_expand_row_inside_button_with_children) {
  ImmTestHarness h;
  auto row = button(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(300), pixels(30)})
                        .with_flex_direction(FlexDirection::Row));
  auto status = div(h.context(), mk(row.ent(), 0),
                    ComponentConfig{}.with_size(
                        ComponentSize{pixels(16), pixels(30)}));
  auto filename = div(h.context(), mk(row.ent(), 1),
                      ComponentConfig{}.with_size(
                          ComponentSize{expand(), pixels(30)}));
  h.layout_only();

  // Written against the button's CONTENT box, not its outer rect: unlike div(),
  // button() carries default padding, so hardcoding 300 - 16 here fails at 252
  // and looks like the reported bug when it is just padding being respected.
  const auto &row_cmp = row.ent().get<UIComponent>();
  const float content_w = rect_of(row).width -
                          row_cmp.computed_padd[Axis::left] -
                          row_cmp.computed_padd[Axis::right];

  CHECK_APPROX(rect_of(filename).width, content_w - 16.f);

  // Both children stay on one line -- the reported failure was the status
  // letter wrapping below because the filename had taken the full width.
  CHECK_APPROX(rect_of(status).y, rect_of(filename).y);
  // And they tile: no gap, no overlap.
  CHECK_APPROX(rect_of(status).x + rect_of(status).width, rect_of(filename).x);
}

// ===========================================================================
// D24 -- the two widgets floatinghotel routes around
//
// Filed as "Known Vendor Bugs" rather than missing features, each with a
// concrete symptom in a shipping app. Neither had any test coverage: the two
// existing tab_container tests are both about label widths, and toggle_switch
// had none at all.
// ===========================================================================

// "tab_container() renders at screen-absolute position, ignoring parent
// container bounds." Reported impact: unusable for multi-repo tabs, so
// floatinghotel hand-builds a row of buttons instead.
//
// The check is positional, not dimensional: nest the bar inside an offset
// panel and it must sit at the PANEL's origin, not the screen's.
TEST(d24_tab_container_respects_parent_origin) {
  ImmTestHarness h;
  // A spacer above and a fixed inset left, so a screen-absolute bar (0,0)
  // is unmistakably distinguishable from a correctly parented one.
  auto column = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(600), pixels(400)})
                        .with_flex_direction(FlexDirection::Column));
  div(h.context(), mk(column.ent(), 0),
      ComponentConfig{}.with_size(ComponentSize{pixels(600), pixels(120)}));
  auto panel = div(h.context(), mk(column.ent(), 1),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(400), pixels(200)}));

  std::vector<std::string> labels{"One", "Two", "Three"};
  size_t active = 0;
  auto bar = tab_container(h.context(), mk(panel.ent(), 0), labels, active,
                           ComponentConfig{}.with_size(
                               ComponentSize{percent(1.0f), pixels(48)}));
  h.layout_only();

  Rectangle p = rect_of(panel), b = rect_of(bar);
  CHECK_APPROX(b.y, p.y); // 120 if parented, 0 if screen-absolute
  CHECK_APPROX(b.x, p.x);
  // And it is bounded by the panel rather than the screen.
  CHECK_APPROX(b.width, p.width);
}

// "toggle_switch() creates sibling entities that consume extra layout space.
// Impact: toggle switches misalign adjacent elements." floatinghotel's
// workaround -- with_no_wrap() on the PARENT plus a taller container -- points
// at the toggle's internals wrapping onto a second line and dragging the row
// taller than the height it was given.
//
// So: a toggle between two rows in a column. If the toggle stays within its
// declared 40px, the row below starts exactly 40px lower.
TEST(d24_toggle_switch_stays_within_its_row) {
  ImmTestHarness h;
  auto column = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(300), pixels(300)})
                        .with_flex_direction(FlexDirection::Column));
  auto above = div(h.context(), mk(column.ent(), 0),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(300), pixels(40)}));
  bool value = false;
  auto toggle = toggle_switch(h.context(), mk(column.ent(), 1), value,
                              ComponentConfig{}
                                  .with_size(ComponentSize{pixels(300),
                                                           pixels(40)})
                                  .with_label("Enable"));
  auto below = div(h.context(), mk(column.ent(), 2),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(300), pixels(40)}));
  h.layout_only();

  CHECK_APPROX(rect_of(toggle).height, 40.f);
  // The row below must not be pushed down by the toggle's internals.
  CHECK_APPROX(rect_of(below).y, rect_of(above).y + 80.f);
}

// The same bar under an ABSOLUTELY-positioned ancestor. This is the condition
// most likely to have produced the original report: c10c0aa fixed
// "percent(1.0f) resolved to screen width inside absolute-positioned parents",
// and a tab bar defaulting to percent(1.0f) width inside an absolute panel is
// exactly that bug wearing a tab_container costume.
TEST(d24_tab_container_under_absolute_parent) {
  ImmTestHarness h;
  auto panel = div(h.context(), mk(h.root(), 0),
                   ComponentConfig{}
                       .with_size(ComponentSize{pixels(400), pixels(200)})
                       .with_absolute_position(150.f, 90.f));

  std::vector<std::string> labels{"One", "Two"};
  size_t active = 0;
  auto bar = tab_container(h.context(), mk(panel.ent(), 0), labels, active,
                           ComponentConfig{}.with_size(
                               ComponentSize{percent(1.0f), pixels(48)}));
  h.layout_only();

  Rectangle p = rect_of(panel), b = rect_of(bar);
  CHECK_APPROX(b.x, p.x); // 150, not 0
  CHECK_APPROX(b.y, p.y); // 90, not 0
  // percent(1.0f) means the PANEL's width, not the 800px screen.
  CHECK_APPROX(b.width, 400.f);
}

// The toggle in a parent too narrow for label + 52px track. floatinghotel's
// workaround was with_no_wrap() on the parent plus a taller container, which
// is what you reach for when contents wrap onto a second line -- so squeeze it
// until that would happen.
TEST(d24_toggle_switch_in_narrow_parent_does_not_wrap) {
  ImmTestHarness h;
  auto column = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(120), pixels(200)})
                        .with_flex_direction(FlexDirection::Column));
  auto above = div(h.context(), mk(column.ent(), 0),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(120), pixels(30)}));
  bool value = true;
  auto toggle = toggle_switch(h.context(), mk(column.ent(), 1), value,
                              ComponentConfig{}
                                  .with_size(ComponentSize{pixels(120),
                                                           pixels(30)})
                                  .with_label("A fairly long setting name"));
  auto below = div(h.context(), mk(column.ent(), 2),
                   ComponentConfig{}.with_size(
                       ComponentSize{pixels(120), pixels(30)}));
  h.layout_only();

  CHECK_APPROX(rect_of(toggle).height, 30.f);
  CHECK_APPROX(rect_of(below).y, rect_of(above).y + 60.f);
}

// ===========================================================================
// D6b -- the render path, now that there is a harness for it
//
// ImmTestHarness::layout_only() runs autolayout and stops, so every test in
// this repo that said "and render" rendered nothing. h.render() drives
// RenderImm for real against the `none` backend, which records draw calls
// instead of touching a GPU.
//
// This file is deliberately NOT in RAYLIB_TESTS: with a real backend the draws
// go to raylib and nothing is recorded.
// ===========================================================================

// Smoke test for the harness itself. If this ever goes quiet, every render
// assertion below is silently vacuous -- which is exactly the trap that made
// the first D6 test worthless.
TEST(d6b_render_harness_actually_draws) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_label("hello")
          .with_custom_background(Color{10, 20, 30, 255}));
  h.render();

  ui_test::check(!h.drawn("rectangle").empty() ||
                     !h.drawn("rectangle_rounded").empty(),
                 "a background rect was drawn", __FILE__, __LINE__);

  auto texts = h.drawn("text");
  ui_test::check(!texts.empty(), "label text was drawn", __FILE__, __LINE__);
  if (!texts.empty())
    ui_test::check(texts[0].text == "hello", "drawn text is the label",
                   __FILE__, __LINE__);
}

// D6: the reported infinite hang. The layout half is already covered by the
// d24 tab tests; this is the render half, which nothing could reach before.
// A label far too wide for its box, with Ellipsis, on an expand()-sized
// element -- the exact combination the report named.
TEST(d6_ellipsis_with_expand_terminates_and_truncates) {
  ImmTestHarness h;
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(120), pixels(20)})
                     .with_flex_direction(FlexDirection::Row));
  const std::string long_label =
      "a really quite long label that cannot possibly fit in 120 pixels";
  div(h.context(), mk(row.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{expand(), pixels(20)})
          .with_label(long_label)
          .with_text_overflow(TextOverflow::Ellipsis));
  h.render();

  // Reaching here at all is the headline: the report says this never returns.
  auto texts = h.drawn("text");
  ui_test::check(!texts.empty(), "truncated label was drawn", __FILE__,
                 __LINE__);
  if (!texts.empty()) {
    const std::string &drawn = texts[0].text;
    ui_test::check(drawn.size() < long_label.size(), "label was shortened",
                   __FILE__, __LINE__);
    ui_test::check(drawn.size() >= 3 &&
                       drawn.compare(drawn.size() - 3, 3, "...") == 0,
                   "shortened label ends in an ellipsis", __FILE__, __LINE__);
  }
}

// Same, with children() sizing -- the report names both.
TEST(d6_ellipsis_with_children_sizing_terminates) {
  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size(
                     ComponentSize{children(), pixels(20)}));
  div(h.context(), mk(box.ent(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{children(), pixels(20)})
          .with_label("another label that is much too long to fit")
          .with_text_overflow(TextOverflow::Ellipsis));
  h.render();

  ui_test::check(!h.drawn("text").empty(), "children()+ellipsis rendered",
                 __FILE__, __LINE__);
}

// ===========================================================================
// D14 -- with_disabled(true) must look disabled, not just act disabled
//
// floatinghotel: "Since real apps overwhelmingly use
// with_custom_background(Color), with_disabled(true) blocks interactions but
// does NOT change the visual appearance." Their workaround is to hand-pick
// different colours in every preset factory.
//
// An accessibility bug rather than a cosmetic one: a control that looks
// enabled invites clicks that silently do nothing.
// ===========================================================================

namespace {
bool same_color(const Color &a, const Color &b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// The widest recorded background rect -- the widget's own fill, as opposed to
// the thin per-side border rects drawn over it.
std::optional<Color> widest_fill(ImmTestHarness &h) {
  std::optional<Color> best;
  float best_w = -1.f;
  for (const char *op : {"rectangle", "rectangle_rounded"}) {
    for (const auto &c : h.drawn(op)) {
      if (c.rect.width > best_w) {
        best_w = c.rect.width;
        best = c.color;
      }
    }
  }
  return best;
}
} // namespace

// The regression test. Fails before the fix: the drawn colour comes back as
// the raw custom colour, undimmed.
TEST(d14_disabled_custom_background_is_dimmed) {
  ImmTestHarness h;
  const Color custom{200, 60, 60, 255};
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_custom_background(custom)
          .with_disabled(true));
  h.render();

  auto fill = widest_fill(h);
  ui_test::check(fill.has_value(), "a background was drawn", __FILE__,
                 __LINE__);
  if (fill) {
    ui_test::check(!same_color(*fill, custom),
                   "disabled custom background is not the raw colour",
                   __FILE__, __LINE__);
    ui_test::check(same_color(*fill, h.context().theme.disabled_variant(custom)),
                   "disabled custom background matches disabled_variant",
                   __FILE__, __LINE__);
  }
}

// The control. Without with_disabled the colour must be untouched -- guards
// against the fix dimming everything.
TEST(d14_enabled_custom_background_is_untouched) {
  ImmTestHarness h;
  const Color custom{200, 60, 60, 255};
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_custom_background(custom));
  h.render();

  auto fill = widest_fill(h);
  ui_test::check(fill.has_value(), "a background was drawn", __FILE__,
                 __LINE__);
  if (fill)
    ui_test::check(same_color(*fill, custom),
                   "enabled custom background is exactly as given", __FILE__,
                   __LINE__);
}

// The theme-usage path already dimmed correctly; this pins it so extracting
// disabled_variant out of from_usage cannot quietly change it.
TEST(d14_disabled_theme_background_still_dims) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_color_usage(Theme::Usage::Primary)
          .with_disabled(true));
  h.render();

  auto fill = widest_fill(h);
  ui_test::check(fill.has_value(), "a background was drawn", __FILE__,
                 __LINE__);
  if (fill) {
    const Theme &theme = h.context().theme;
    ui_test::check(
        same_color(*fill, theme.from_usage(Theme::Usage::Primary, true)),
        "disabled theme background still dims", __FILE__, __LINE__);
    ui_test::check(
        !same_color(*fill, theme.from_usage(Theme::Usage::Primary, false)),
        "disabled theme background differs from enabled", __FILE__, __LINE__);
  }
}

TEST(d14_theme_derived_disabled_text_still_dims) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_label("disabled")
          .with_disabled(true));
  h.render();

  auto texts = h.drawn("text");
  ui_test::check(!texts.empty(), "disabled label was drawn", __FILE__,
                 __LINE__);
  if (texts.empty())
    return;
  const Color disabled_ink = texts[0].color;

  ImmTestHarness h2;
  div(h2.context(), mk(h2.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_label("enabled"));
  h2.render();
  auto enabled_texts = h2.drawn("text");
  ui_test::check(!enabled_texts.empty(), "enabled label was drawn", __FILE__,
                 __LINE__);
  if (enabled_texts.empty())
    return;
  ui_test::check(!same_color(disabled_ink, enabled_texts[0].color),
                 "a theme-derived disabled label still dims", __FILE__,
                 __LINE__);
  ui_test::check(
      same_color(disabled_ink, colors::darken(enabled_texts[0].color, 0.5f)),
      "and it dims by the engine's own transform", __FILE__, __LINE__);
}

TEST(d14_explicit_disabled_text_color_is_preserved) {
  ImmTestHarness h;
  const Color text_color{240, 240, 240, 255};
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(40)})
          .with_label("disabled")
          .with_custom_text_color(text_color)
          .with_disabled(true));
  h.render();

  auto texts = h.drawn("text");
  ui_test::check(!texts.empty(), "disabled label was drawn", __FILE__,
                 __LINE__);
  if (texts.empty())
    return;
  ui_test::check(same_color(texts[0].color, text_color),
                 "an explicit disabled text colour is drawn as given",
                 __FILE__, __LINE__);
  ui_test::check(!same_color(texts[0].color, colors::darken(text_color, 0.5f)),
                 "an explicit disabled text colour is not halved", __FILE__,
                 __LINE__);
}

TEST(d14_disabled_stays_disabled_for_input) {
  ImmTestHarness h;
  auto result = button(h.context(), mk(h.root(), 0),
                       ComponentConfig{}
                           .with_size(ComponentSize{pixels(200), pixels(40)})
                           .with_label("disabled")
                           .with_custom_text_color(Color{240, 240, 240, 255})
                           .with_disabled(true));
  h.render();

  const Entity &e = result.ent();
  ui_test::check(e.has<HasLabel>(), "the button has a label", __FILE__,
                 __LINE__);
  if (!e.has<HasLabel>())
    return;
  ui_test::check(e.get<HasLabel>().is_disabled,
                 "an explicit text colour does not clear is_disabled",
                 __FILE__, __LINE__);
  ui_test::check(!static_cast<bool>(result),
                 "a disabled button does not report activation", __FILE__,
                 __LINE__);
}

// ===========================================================================
// #22 / #24 -- styled spans and hard line breaks
//
// Written BEFORE the fix, so each one is proven to fail against today's code.
// A test that is green both before and after proves nothing; that has already
// bitten twice in this file's history (a harness that never rendered, then a
// measure_text stub that returned {0,0}).
//
// #22: with_styled_label spans draw on ONE line, so styled runs
//      cannot be used inside a wrapping paragraph. Also -- not in the
//      original report -- spans are handled at exactly one site in rendering.h, inside
//      RenderBatched, so with_styled_label is a silent no-op under RenderImm.
//      Both renderers are runtime-selectable (utilities.h, `use_batched`).
//
// #24: detail::wrap_text_to_width splits on ' ' only, so '\n' is an
//      ordinary word character. Multi-line bodies collapse into one run-on
//      paragraph and the caller's height model (sized for N logical lines) is
//      wrong, leaving a large empty gap.
// ===========================================================================

namespace {
// Recorded text draws grouped into visual lines: same y == same line, runs on
// a line concatenated in paint order, lines ordered top to bottom. Works for
// both the plain wrapper (one draw per line) and spans (one draw per run).
std::vector<std::string> drawn_lines(ImmTestHarness &h) {
  std::vector<std::pair<float, std::string>> acc;
  for (const auto &c : h.drawn("text")) {
    bool merged = false;
    for (auto &p : acc) {
      if (std::fabs(p.first - c.rect.y) < 0.5f) {
        p.second += c.text;
        merged = true;
        break;
      }
    }
    if (!merged)
      acc.push_back({c.rect.y, c.text});
  }
  std::sort(acc.begin(), acc.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  std::vector<std::string> out;
  for (auto &p : acc)
    out.push_back(p.second);
  return out;
}

const char *kParagraph =
    "the quick brown fox jumps over the lazy dog and keeps on running "
    "well past the edge of the box";
} // namespace

// #22: a styled label long enough to need wrapping must wrap AND stay styled.
//
// The colour assertion is the load-bearing half. Today the wrap branch
// (rendering.h:2173) runs first and sets `wrapped`, which makes the span
// branch at :2209 unreachable -- so a wrapping styled label silently renders
// as PLAIN text. Checking only the line count would pass on that fallback and
// prove nothing.
TEST(d22_styled_spans_wrap_and_stay_styled) {
  const Color teal{0, 190, 190, 255};
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(240), pixels(120)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_text_overflow(TextOverflow::Wrap)
          .with_styled_label({TextSpan{kParagraph, teal}}));
  h.render_batched();

  auto lines = drawn_lines(h);
  ui_test::check(lines.size() > 1, "styled label wrapped onto >1 line",
                 __FILE__, __LINE__);

  bool all_teal = !h.drawn("text").empty();
  for (const auto &c : h.drawn("text"))
    if (!same_color(c.color, teal))
      all_teal = false;
  ui_test::check(all_teal, "wrapped styled label kept its span colour",
                 __FILE__, __LINE__);
}

// #22: the invariant that makes styled labels usable -- a caller sizes a box
// from the plain wrapper's line count, so styled must wrap on exactly the same
// boundaries. letter_spacing is set because that is where a per-word-width
// implementation drifts from measuring the whole candidate line.
//
// This one PASSES today, for the wrong reason: a wrapping styled label falls
// back to drawing the plain concatenated label, so the two are trivially
// identical. It earns its keep AFTER the fix, as the guard against the
// per-word-width drift that made the proposed patch unsafe to take as-is.
TEST(d22_styled_wraps_on_same_boundaries_as_plain) {
  std::vector<std::string> plain_lines;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(240), pixels(120)})
            .with_font(UIComponent::DEFAULT_FONT, 16.f)
            .with_letter_spacing(2.f)
            .with_text_overflow(TextOverflow::Wrap)
            .with_label(kParagraph));
    h.render_batched();
    plain_lines = drawn_lines(h);
  }

  std::vector<std::string> span_lines;
  {
    ImmTestHarness h;
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(240), pixels(120)})
            .with_font(UIComponent::DEFAULT_FONT, 16.f)
            .with_letter_spacing(2.f)
            .with_text_overflow(TextOverflow::Wrap)
            .with_styled_label(
                {TextSpan{kParagraph, Color{255, 255, 255, 255}}}));
    h.render_batched();
    span_lines = drawn_lines(h);
  }

  ui_test::check(plain_lines.size() > 1, "plain paragraph wrapped", __FILE__,
                 __LINE__);
  ui_test::check(span_lines == plain_lines,
                 "styled wraps on the same boundaries as plain", __FILE__,
                 __LINE__);
}

// #22: each run keeps its own colour after wrapping.
TEST(d22_wrapped_spans_keep_their_colors) {
  const Color red{220, 60, 60, 255};
  const Color blue{60, 120, 220, 255};
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(240), pixels(120)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_text_overflow(TextOverflow::Wrap)
          .with_styled_label({
              TextSpan{"the quick brown fox jumps over ", red},
              TextSpan{"the lazy dog and keeps on running far", blue},
          }));
  h.render_batched();

  bool saw_red = false, saw_blue = false;
  for (const auto &c : h.drawn("text")) {
    if (same_color(c.color, red))
      saw_red = true;
    if (same_color(c.color, blue))
      saw_blue = true;
  }
  ui_test::check(saw_red && saw_blue, "both span colours survive wrapping",
                 __FILE__, __LINE__);
  ui_test::check(drawn_lines(h).size() > 1, "multi-span label wrapped",
                 __FILE__, __LINE__);
}

// #22: spans are only implemented in RenderBatched, so with_styled_label draws
// nothing coloured under RenderImm -- the same UI renders differently
// depending on a runtime flag no app-level code mentions.
TEST(d22_styled_spans_render_under_render_imm_too) {
  const Color red{220, 60, 60, 255};
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(240), pixels(60)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_styled_label({TextSpan{"hello", red}}));
  h.render();

  bool saw_red = false;
  for (const auto &c : h.drawn("text"))
    if (same_color(c.color, red))
      saw_red = true;
  ui_test::check(saw_red, "RenderImm honours span colours", __FILE__, __LINE__);
}

// #24: '\n' is a hard break. Today it is an ordinary word character, so this
// draws one run-on line instead of two.
TEST(d24_newline_forces_a_line_break) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(400), pixels(120)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_text_overflow(TextOverflow::Wrap)
          .with_label("first line\nsecond line"));
  h.render_batched();

  auto lines = drawn_lines(h);
  ui_test::check(lines.size() == 2, "newline split the label into two lines",
                 __FILE__, __LINE__);
  if (lines.size() == 2) {
    ui_test::check(lines[0] == "first line", "first line is intact", __FILE__,
                   __LINE__);
    ui_test::check(lines[1] == "second line", "second line is intact",
                   __FILE__, __LINE__);
  }
}

// #24: a blank line between paragraphs must survive, otherwise prose that
// relies on '\n\n' for spacing renders as one block.
TEST(d24_blank_line_between_paragraphs_is_preserved) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(400), pixels(200)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_text_overflow(TextOverflow::Wrap)
          .with_label("para one\n\npara two"));
  h.render_batched();

  auto lines = drawn_lines(h);
  // Two drawn lines with a full line-height gap between them: the empty middle
  // line advances the pen without emitting text.
  ui_test::check(lines.size() == 2, "two paragraphs drawn", __FILE__,
                 __LINE__);
  auto texts = h.drawn("text");
  if (texts.size() >= 2) {
    float gap = texts.back().rect.y - texts.front().rect.y;
    ui_test::check(gap > 16.f * 1.5f,
                   "blank line leaves a full line of vertical space", __FILE__,
                   __LINE__);
  }
}

// ===========================================================================
// #22b -- with_font_size() alone is silently ignored
//
// Not from any app report; found while writing the #22/#24 tests, when a
// plain-text WRAP baseline that should obviously have passed did not.
//
// component_init.h:356 gates enable_font() -- the ONLY thing that copies
// font_size and font_size_explicitly_set onto the UIComponent -- on
// `config.font_name != UNSET_FONT`. So calling with_font_size() without also
// naming a font leaves cmp.font_size_explicitly_set false, explicit_fs
// resolves to 0, and every code path that requires a known font size quietly
// switches off. Wrapping is the visible casualty: rendering.h:2174 requires
// explicit_fs > 0.
//
// Strong candidate for floatinghotel footgun F1 ("labels don't word-wrap") --
// set a size, ask for wrapping, get one long clipped line and no diagnostic.
// ===========================================================================
TEST(d22b_with_font_size_alone_enables_wrapping) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(240), pixels(120)})
          .with_font_size(16.f) // deliberately NO with_font()
          .with_text_overflow(TextOverflow::Wrap)
          .with_label(kParagraph));
  h.render_batched();

  ui_test::check(drawn_lines(h).size() > 1,
                 "with_font_size alone is enough to wrap", __FILE__, __LINE__);
}

// The same widget WITH a font name wraps, proving the size was never the
// problem -- it is the name gate. Passes today; guards the fix's blast radius.
TEST(d22b_with_font_name_and_size_wraps_today) {
  ImmTestHarness h;
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(240), pixels(120)})
          .with_font(UIComponent::DEFAULT_FONT, 16.f)
          .with_text_overflow(TextOverflow::Wrap)
          .with_label(kParagraph));
  h.render_batched();

  ui_test::check(drawn_lines(h).size() > 1, "named font + size wraps",
                 __FILE__, __LINE__);
}

// #22: the two renderers must agree. `use_batched` is a runtime flag that no
// app-level UI code mentions, so a styled label that renders differently
// across it is a trap. Also the real check on RenderImm's per-run inset
// compensation: draw_text_in_rect insets by 5px, so a tight run rect would
// shift every run right relative to the batched path.
TEST(d22_both_renderers_agree_on_styled_output) {
  const Color red{220, 60, 60, 255};
  const Color blue{60, 120, 220, 255};
  const auto build = [&](ImmTestHarness &h) {
    div(h.context(), mk(h.root(), 0),
        ComponentConfig{}
            .with_size(ComponentSize{pixels(240), pixels(120)})
            .with_font(UIComponent::DEFAULT_FONT, 16.f)
            .with_text_overflow(TextOverflow::Wrap)
            .with_styled_label({
                TextSpan{"the quick brown fox jumps over ", red},
                TextSpan{"the lazy dog and keeps on running far", blue},
            }));
  };

  std::vector<DrawCall> imm_texts, batched_texts;
  {
    ImmTestHarness h;
    build(h);
    h.render();
    imm_texts = h.drawn("text");
  }
  {
    ImmTestHarness h;
    build(h);
    h.render_batched();
    batched_texts = h.drawn("text");
  }

  ui_test::check(!imm_texts.empty(), "RenderImm drew the styled label",
                 __FILE__, __LINE__);
  ui_test::check(imm_texts.size() == batched_texts.size(),
                 "both renderers emit the same number of runs", __FILE__,
                 __LINE__);
  if (imm_texts.size() == batched_texts.size()) {
    bool same = true;
    for (size_t i = 0; i < imm_texts.size(); i++) {
      if (imm_texts[i].text != batched_texts[i].text ||
          !same_color(imm_texts[i].color, batched_texts[i].color))
        same = false;
    }
    ui_test::check(same, "both renderers emit the same run text and colours",
                   __FILE__, __LINE__);
  }
}

// ===========================================================================
// D4 -- FlexWrap defaults to Wrap
//
// Reported as "the single nastiest default we hit": a Column taller than its
// viewport silently wraps its children into a SECOND column off to the right
// instead of overflowing, so you get stray content hugging the right edge and
// no diagnostic. Every scroll container and every stacking Column needs an
// explicit .with_no_wrap() today.
//
// Flipping the default has two traps worth pinning:
//   - there is no with_wrap(), only with_no_wrap(), so wrapping would become
//     unreachable from ComponentConfig
//   - apply_overrides uses FlexWrap::Wrap as its "caller did not set this"
//     sentinel, which inverts when the default moves
// ===========================================================================

// A column whose children overflow it must stay a single column.
TEST(d4_tall_column_does_not_wrap_into_a_second_column) {
  ImmTestHarness h;
  auto col = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(100), pixels(100)})
                     .with_flex_direction(FlexDirection::Column));
  std::vector<ElementResult> kids;
  for (int i = 0; i < 4; i++)
    kids.push_back(div(h.context(), mk(col.ent(), i),
                       ComponentConfig{}.with_size(
                           ComponentSize{pixels(100), pixels(50)})));
  h.layout_only();

  const float left = rect_of(col).x;
  bool all_in_one_column = true;
  for (const auto &k : kids)
    if (!ui_test::approx(rect_of(k).x, left))
      all_in_one_column = false;
  ui_test::check(all_in_one_column,
                 "overflowing column children stay in one column", __FILE__,
                 __LINE__);
}

// Wrapping must stay reachable, and actually wrap, when asked for explicitly.
TEST(d4_explicit_wrap_still_wraps) {
  ImmTestHarness h;
  auto row = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}
                     .with_size(ComponentSize{pixels(100), pixels(100)})
                     .with_flex_direction(FlexDirection::Row)
                     .with_wrap());
  std::vector<ElementResult> kids;
  for (int i = 0; i < 4; i++)
    kids.push_back(div(h.context(), mk(row.ent(), i),
                       ComponentConfig{}.with_size(
                           ComponentSize{pixels(50), pixels(25)})));
  h.layout_only();

  bool saw_second_row = false;
  const float top = rect_of(kids[0]).y;
  for (const auto &k : kids)
    if (rect_of(k).y > top + 1.f)
      saw_second_row = true;
  ui_test::check(saw_second_row, "with_wrap() still wraps onto a second line",
                 __FILE__, __LINE__);
}

// The override sentinel: asking for the non-default value in a restyle has to
// survive the merge.
TEST(d4_wrap_override_survives_apply_overrides) {
  ComponentConfig base;
  ComponentConfig overrides;
  overrides.with_wrap();
  const ComponentConfig merged = base.apply_overrides(overrides);
  ui_test::check(merged.flex_wrap == FlexWrap::Wrap,
                 "an explicit wrap override is applied", __FILE__, __LINE__);
}

// D13: an open dropdown always opened downward, so one near the bottom edge
// ran off screen. The tray now goes through overlay::place().
namespace {
UIComponent *open_dropdown_tray(ImmTestHarness &h, float y) {
  std::vector<std::string> opts = {"One", "Two", "Three", "Four"};
  size_t idx = 0;
  auto emit = [&] {
    dropdown(h.context(), mk(h.root(), 0), opts, idx,
             ComponentConfig{}
                 .with_size(ComponentSize{pixels(160), pixels(30)})
                 .with_absolute_position(40.f, y));
  };
  h.begin_frame();
  emit();
  h.layout_only();
  for (const auto &e : UICollectionHolder::get().collection.get_entities())
    if (e && e->has<HasDropdownState>())
      e->get<HasDropdownState>().on = true;
  h.begin_frame();
  emit();
  h.layout_only();
  return h.find("dropdown_options_tray");
}
} // namespace

TEST(d13_dropdown_opens_downward_with_room) {
  ImmTestHarness h;
  UIComponent *t = open_dropdown_tray(h, 50.f);
  ui_test::check(t != nullptr, "tray exists", __FILE__, __LINE__);
  if (t)
    ui_test::check(t->rect().y > 50.f, "tray sits below the trigger",
                   __FILE__, __LINE__);
}

TEST(d13_dropdown_flips_up_near_the_bottom) {
  ImmTestHarness h;
  UIComponent *t = open_dropdown_tray(h, 560.f);
  ui_test::check(t != nullptr, "tray exists", __FILE__, __LINE__);
  if (t) {
    const float y = t->rect().y;
    ui_test::check(y < 560.f, "tray flips above the trigger", __FILE__,
                   __LINE__);
    ui_test::check(y >= 0.f && y + t->rect().height <= 601.f,
                   "tray stays on screen", __FILE__, __LINE__);
    if (y >= 560.f)
      fprintf(stderr, "        tray y=%.1f h=%.1f\n", y, t->rect().height);
  }
}

// ===========================================================================
// D33/D34 -- the focus ring, reported by `puzzle`
//
// puzzle hand-painted its entire menu focus ring rather than use the theme's,
// and the two reasons are here. Note these were only testable once the `none`
// backend stopped dropping draw_rectangle_rounded_lines on the floor -- every
// outline in the library (borders included) was invisible to this harness.
// ===========================================================================

namespace {
// A focused, opaque, borderless button. `focus_col` is what the ring is drawn
// in; nothing else in the widget uses it, so it identifies the ring.
constexpr Color kFocusCol{255, 0, 255, 255};
constexpr Color kFillCol{10, 20, 30, 255};

ElementResult focused_button(ImmTestHarness &h, float ring_thickness) {
  h.context().theme.focus = kFocusCol;
  h.context().theme.focus_ring_thickness = ring_thickness;
  h.context().theme.focus_ring_offset = 4.f;
  auto b = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(200), pixels(40)})
                      .with_custom_background(kFillCol)
                      .with_debug_name("focus_target"));
  h.context().focus_id = b.ent().id;
  h.context().visual_focus_id = b.ent().id;
  return b;
}

// Every outline on a borderless widget comes from the focus-ring block -- and
// counting ALL of them, not just the ones in theme.focus, is deliberate: the
// contrast outline is drawn in black/white and unconditionally, so a
// focus-colour-only count would call thickness=0 "off" while it still paints a
// line into the widget.
int count_ring_draws(const std::vector<ui_test::DrawCall> &calls) {
  int n = 0;
  for (const auto &c : calls)
    if (c.op == "rectangle_rounded_lines")
      n++;
  return n;
}
} // namespace

// D33: theme.focus_ring_thickness = 0 is the only way to ask for no ring, and
// it does not work. The batched path routes 0 to render_rounded_outline_batch's
// `else` branch (thickness > 1.0f is false) and draws a 1px line anyway, and
// both paths draw the contrast outline unconditionally. puzzle set 0 to opt out
// and shipped a doubled ring without noticing.
TEST(d33_zero_thickness_disables_the_ring_batched) {
  ImmTestHarness h;
  focused_button(h, 0.f);
  const auto &calls = h.render_batched();
  ui_test::check(count_ring_draws(calls) == 0,
                 "focus_ring_thickness = 0 draws no ring (batched)", __FILE__,
                 __LINE__);
}

TEST(d33_zero_thickness_disables_the_ring_immediate) {
  ImmTestHarness h;
  focused_button(h, 0.f);
  const auto &calls = h.render();
  ui_test::check(count_ring_draws(calls) == 0,
                 "focus_ring_thickness = 0 draws no ring (immediate)", __FILE__,
                 __LINE__);
}

// ...and a non-zero thickness must still draw one, or the fix above is just a
// way to delete the feature.
TEST(d33_nonzero_thickness_still_draws_a_ring) {
  ImmTestHarness h;
  focused_button(h, 3.f);
  ui_test::check(count_ring_draws(h.render_batched()) > 0,
                 "a ring is drawn when thickness > 0 (batched)", __FILE__,
                 __LINE__);
}

TEST(d33_nonzero_thickness_still_draws_a_ring_immediate) {
  ImmTestHarness h;
  focused_button(h, 3.f);
  ui_test::check(count_ring_draws(h.render()) > 0,
                 "a ring is drawn when thickness > 0 (immediate)", __FILE__,
                 __LINE__);
}

// D34: the immediate renderer draws the ring BEFORE the widget's own fill, and
// focus_rect is inset inside the fill rect -- so any opaque background paints
// over the ring completely. That is every default Button. The batched path is
// fine because it emits the ring at layer+199/+200.
TEST(d34_immediate_ring_draws_over_the_fill_not_under_it) {
  ImmTestHarness h;
  focused_button(h, 3.f);
  const auto &calls = h.render();

  int last_fill = -1, first_ring = -1;
  for (int i = 0; i < (int)calls.size(); i++) {
    const auto &c = calls[(size_t)i];
    const bool is_fill = (c.op == "rectangle_rounded" || c.op == "rectangle") &&
                         c.color.r == kFillCol.r && c.color.g == kFillCol.g &&
                         c.color.b == kFillCol.b;
    const bool is_ring = c.op == "rectangle_rounded_lines" &&
                         c.color.r == kFocusCol.r && c.color.g == kFocusCol.g &&
                         c.color.b == kFocusCol.b;
    if (is_fill)
      last_fill = i;
    if (is_ring && first_ring < 0)
      first_ring = i;
  }

  ui_test::check(last_fill >= 0, "the widget fill was drawn", __FILE__,
                 __LINE__);
  ui_test::check(first_ring >= 0, "the focus ring was drawn", __FILE__,
                 __LINE__);
  if (last_fill >= 0 && first_ring >= 0)
    ui_test::check(first_ring > last_fill,
                   "focus ring is drawn after the fill, so it is visible",
                   __FILE__, __LINE__);
}

// ===========================================================================
// D35 -- hover-follow silently overwrites an explicit set_focus
//
// Reported by `puzzle`: on a menu with the cursor resting anywhere over the
// list, the arrow keys did nothing. ComputeVisualFocusId runs AFTER every user
// system, so a game that moves focus during its build has that move undone the
// same frame. There was no way to say "the keyboard just did this".
//
// ComputeVisualFocusId reads the context through the EntityHelper singleton
// (default collection) while widgets live in the UI collection, so the context
// here is a separate entity from the harness's.
// ===========================================================================

using ui_test::TestInputAction;

namespace {
UIContext<TestInputAction> &singleton_ctx() {
  if (!EntityHelper::has_singleton<UIContext<TestInputAction>>()) {
    Entity &e = EntityHelper::createPermanentEntity();
    e.addComponent<UIContext<TestInputAction>>();
    EntityHelper::registerSingleton<UIContext<TestInputAction>>(e);
  }
  auto *c = EntityHelper::get_singleton_cmp<UIContext<TestInputAction>>();
  c->reset(); // fresh per test (UIContext is not copy-assignable)
  c->mouse = MousePointerState{};
  c->focus_source = FocusSource::Grab;
  c->theme.highlight_mode = HighlightMode::FollowsMostRecentInput;
  return *c;
}

// Two focusable buttons, laid out and marked rendered.
std::pair<EntityID, EntityID> two_buttons(ImmTestHarness &h) {
  auto a = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(100), pixels(20)})
                      .with_debug_name("btn_a"));
  auto b = button(h.context(), mk(h.root(), 1),
                  ComponentConfig{}
                      .with_size(ComponentSize{pixels(100), pixels(20)})
                      .with_debug_name("btn_b"));
  h.layout_only();
  return {a.ent().id, b.ent().id};
}
} // namespace

// The keyboard moved focus to A this frame while the cursor sits on B. A wins.
TEST(d35_explicit_set_focus_beats_hover) {
  ImmTestHarness h;
  auto [a, b] = two_buttons(h);
  auto &ctx = singleton_ctx();
  ctx.hot_id = b;
  ctx.mouse.moved_this_frame = true;
  ctx.set_focus(a); // what a game's own nav system does

  ComputeVisualFocusId<ui_test::TestInputAction> sys;
  sys.for_each_with(h.context_entity(), 0.f);

  ui_test::check(ctx.focus_id == a,
                 "an explicit set_focus this frame survives hover-follow",
                 __FILE__, __LINE__);
}

// ...but hover-follow must still work when nothing claimed focus explicitly,
// or this fix just deletes FollowsMostRecentInput. try_to_grab is the ordinary
// per-frame re-grab and must NOT count as explicit -- it runs every frame, so
// treating it as intent would disable hover for good.
TEST(d35_hover_still_follows_when_focus_was_only_regrabbed) {
  ImmTestHarness h;
  auto [a, b] = two_buttons(h);
  auto &ctx = singleton_ctx();
  ctx.hot_id = b;
  ctx.mouse.moved_this_frame = true;
  ctx.focus_id = a;
  ctx.try_to_grab(a);

  ComputeVisualFocusId<ui_test::TestInputAction> sys;
  sys.for_each_with(h.context_entity(), 0.f);

  ui_test::check(ctx.focus_id == b, "hover still claims focus after a re-grab",
                 __FILE__, __LINE__);
}

// A stationary cursor must not read as movement. The compare was exact, so
// sub-pixel jitter counted as intent -- and a NaN position (no cursor at all,
// which is every headless test) counted as moving every frame forever.
TEST(d35_tiny_mouse_movement_is_not_movement) {
  UIContext<TestInputAction> ctx;
  ctx.mouse.pos = {100.f, 100.f};
  ui_test::check(!ctx.mouse.moved_since({100.5f, 100.f}),
                 "half a pixel is not a move", __FILE__, __LINE__);
  ui_test::check(ctx.mouse.moved_since({140.f, 100.f}),
                 "forty pixels is a move", __FILE__, __LINE__);

  // No cursor at all is not movement...
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ctx.mouse.pos = {nan, nan};
  ui_test::check(!ctx.mouse.moved_since({100.f, 100.f}),
                 "a NaN cursor position does not read as movement", __FILE__,
                 __LINE__);

  // ...but a cursor appearing after a NaN frame is, or one bad read would
  // pin the previous position and hover-follow would never fire again.
  ctx.mouse.pos = {100.f, 100.f};
  ui_test::check(ctx.mouse.moved_since({nan, nan}),
                 "a cursor appearing after NaN counts as movement", __FILE__,
                 __LINE__);
}

// ===========================================================================
// D36 -- arrows are welded to Tab
//
// process_tabbing assigns WidgetDown into the same `forward` that WidgetNext
// sets, so Tab and Down are one code path and no app can have "arrows move
// within a group, Tab moves between groups". The escape hatch is
// ConsumesDirectionalInput; under its old name (AcceptsValueInput) nothing ever
// attached it and there was no config setter -- three references in the whole
// library, all declarations. Note there is no ValueUp/ValueDown action pair:
// navigation and value adjustment share WidgetUp/WidgetDown, and this component
// is the only thing separating them.
//
// The shipped casualty: a Column tray (which is what dropdown options are)
// cannot be arrow-navigated AT ALL, because process_tabbing consumes WidgetDown
// nine systems before HandleTrayNavigation looks for it.
// ===========================================================================

TEST(d36_tray_keeps_its_own_arrow_keys) {
  ImmTestHarness h;
  auto t = tray(h.context(), mk(h.root(), 0),
                ComponentConfig{}
                    .with_size(ComponentSize{pixels(120), pixels(90)})
                    .with_flex_direction(FlexDirection::Column)
                    .with_debug_name("opts"));
  h.layout_only();

  auto &ctx = h.context();
  ctx.set_focus(t.ent().id);
  ctx.last_action = ui_test::TestInputAction::WidgetDown;
  ctx.process_tabbing(t.ent().id);

  ui_test::check(ctx.focus_id == t.ent().id,
                 "a tray keeps WidgetDown instead of tabbing away", __FILE__,
                 __LINE__);
}

// The default must not change: an ordinary widget still tabs on Down, which is
// what every existing app relies on.
TEST(d36_plain_widget_still_tabs_on_down) {
  ImmTestHarness h;
  auto b = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size(
                      ComponentSize{pixels(100), pixels(20)}));
  h.layout_only();

  auto &ctx = h.context();
  ctx.set_focus(b.ent().id);
  ctx.last_action = ui_test::TestInputAction::WidgetDown;
  ctx.process_tabbing(b.ent().id);

  ui_test::check(ctx.focus_id == ctx.ROOT,
                 "Down still releases focus for the next widget to grab",
                 __FILE__, __LINE__);
}

// An app that owns its own directional nav can turn the folding off wholesale
// rather than racing process_tabbing for a single-slot last_action.
TEST(d36_theme_can_stop_arrows_tabbing) {
  ImmTestHarness h;
  auto b = button(h.context(), mk(h.root(), 0),
                  ComponentConfig{}.with_size(
                      ComponentSize{pixels(100), pixels(20)}));
  h.layout_only();

  auto &ctx = h.context();
  ctx.theme.arrows_tab = false;
  ctx.set_focus(b.ent().id);
  ctx.last_action = ui_test::TestInputAction::WidgetDown;
  ctx.process_tabbing(b.ent().id);

  ui_test::check(ctx.focus_id == b.ent().id,
                 "theme.arrows_tab = false leaves Down to the app", __FILE__,
                 __LINE__);

  // Tab itself must still work with arrows_tab off.
  ctx.last_action = ui_test::TestInputAction::WidgetNext;
  ctx.process_tabbing(b.ent().id);
  ui_test::check(ctx.focus_id == ctx.ROOT, "Tab still tabs", __FILE__,
                 __LINE__);
}

int main() { return ui_test::run_registered_tests("Downstream Gaps"); }
