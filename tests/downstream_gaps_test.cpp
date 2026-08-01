// downstream_gaps_test.cpp
// Executable repros for the gaps hanabi and floatinghotel reported against
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
//   hanabi #18:        "no flex-grow: can't pin a trailing element to the
//                       right edge"
//   floatinghotel:     "Row Flex Layout Broken with expand() Children" --
//                       "any child sized with expand() consumes the full
//                       parent width instead of the remaining width after
//                       fixed-size siblings"
//
// Both paid real cost: hanabi hand-computes
// `labelW = rowContentW - leadSlot - countColW` across three row types;
// floatinghotel bakes whole rows into a single label string and gives up on
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
  h.layout_and_render();

  CHECK_APPROX(rect_of(icon).width, 18.f);
  // 300 - 18 - 24. The whole complaint is that this comes back as 300.
  CHECK_APPROX(rect_of(label).width, 258.f);
  CHECK_APPROX(rect_of(count).width, 24.f);

  // The point of the exercise: one shared right edge, no pixel bookkeeping.
  Rectangle r = rect_of(row), c = rect_of(count);
  CHECK_APPROX(c.x + c.width, r.x + r.width);
}

// hanabi's actual case rather than the reduced one: the row is percent-sized
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
  h.layout_and_render();

  CHECK_APPROX(rect_of(label).width, 358.f); // 400 - 18 - 24

  Rectangle r = rect_of(row), c = rect_of(count);
  CHECK_APPROX(c.x + c.width, r.x + r.width);
}

// Two rows of different composition must land their trailing counts on the
// SAME right edge. This is the assertion hanabi could not make: its smart-view
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
  h.layout_and_render();

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
  h.layout_and_render();

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
  h.layout_and_render();

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
  h.layout_and_render();

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
  h.layout_and_render();

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
  h.layout_and_render();

  CHECK_APPROX(rect_of(toggle).height, 30.f);
  CHECK_APPROX(rect_of(below).y, rect_of(above).y + 60.f);
}

int main() { return ui_test::run_registered_tests("Downstream Gaps"); }
