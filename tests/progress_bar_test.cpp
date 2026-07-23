// progress_bar_test.cpp
// Regression tests for the imm::progress_bar widget.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/progress_bar_test.cpp -o /tmp/t && /tmp/t
//
// Covers two sizing bugs where child overlays compounded percent sizes:
//   - the track was sized with config.size while being a child of the (already
//     config.size) progress_bar entity, so percent(0.7) became 0.7*0.7
//   - the fill/label were sized with config.size against the track, compounding
//     again — the fill ended up shorter/narrower than and offset from the track
// Correct behavior: track fills the entity; fill fills the track height and is
// `normalized` of the track width.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// A percent-sized progress bar: track fills the entity, fill fills the track
// height and takes `normalized` of the track width.
TEST(progress_bar_percent_sizing_fills_track) {
  ImmTestHarness h;

  // Row 400x100; bar is 50% x 70% -> track 200 x 70.
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(100)})
                        .with_debug_name("meter_row"));
  progress_bar(h.context(), mk(row.ent(), 0), 0.5f,
               ComponentConfig{}
                   .with_size(ComponentSize{percent(0.5f), percent(0.7f)})
                   .with_debug_name("pb"),
               ProgressBarLabelStyle::Percentage);
  h.layout_and_render();

  UIComponent *track = h.find("progress_track");
  UIComponent *fill = h.find("progress_fill");
  CHECK(track != nullptr);
  CHECK(fill != nullptr);
  if (track && fill) {
    Rectangle tr = track->rect();
    Rectangle fr = fill->rect();
    CHECK_APPROX(tr.width, 200.f);  // 50% of 400
    CHECK_APPROX(tr.height, 70.f);  // 70% of 100 (not 0.7*0.7*100)
    CHECK_APPROX(fr.height, tr.height);      // fill fills the track height
    CHECK_APPROX(fr.width, tr.width * 0.5f); // fill = normalized * track width
  }
}

// A pixel-sized progress bar keeps working (the fix must not regress it).
TEST(progress_bar_pixel_sizing_fills_track) {
  ImmTestHarness h;

  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(60)})
                        .with_debug_name("row"));
  progress_bar(h.context(), mk(row.ent(), 0), 0.25f,
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(300), pixels(28)})
                   .with_debug_name("pb"),
               ProgressBarLabelStyle::Percentage);
  h.layout_and_render();

  UIComponent *track = h.find("progress_track");
  UIComponent *fill = h.find("progress_fill");
  CHECK(track != nullptr);
  CHECK(fill != nullptr);
  if (track && fill) {
    Rectangle tr = track->rect();
    Rectangle fr = fill->rect();
    CHECK_APPROX(tr.height, 28.f);
    CHECK_APPROX(fr.height, tr.height);
    CHECK_APPROX(fr.width, tr.width * 0.25f);
  }
}

// A full (100%) bar's fill spans the whole track width.
TEST(progress_bar_full_value_fills_width) {
  ImmTestHarness h;

  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  progress_bar(h.context(), mk(row.ent(), 0), 1.0f,
               ComponentConfig{}
                   .with_size(ComponentSize{percent(1.0f), pixels(28)})
                   .with_debug_name("pb"),
               ProgressBarLabelStyle::Percentage);
  h.layout_and_render();

  UIComponent *track = h.find("progress_track");
  UIComponent *fill = h.find("progress_fill");
  CHECK(track != nullptr);
  CHECK(fill != nullptr);
  if (track && fill)
    CHECK_APPROX(fill->rect().width, track->rect().width);
}

int main() { return ui_test::run_registered_tests("progress_bar tests"); }
