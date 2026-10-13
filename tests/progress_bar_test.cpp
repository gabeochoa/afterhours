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

#include <limits>

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
  h.layout_only();

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
  h.layout_only();

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

// At 1.0 the bar is the coloured track alone. Asserting a fill exists pinned
// the implementation; what matters is one full-width box.
TEST(progress_bar_full_value_is_one_full_width_box) {
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
  h.layout_only();

  UIComponent *track = h.find("progress_track");
  UIComponent *fill = h.find("progress_fill");
  CHECK(track != nullptr);
  CHECK(fill == nullptr); // no second box on top of an identical one
  if (track)
    CHECK_APPROX(track->rect().width, 400.f);
}

// Non-finite input draws empty and labels "—" (no NaN cast to int).
TEST(progress_bar_nonfinite_draws_empty_with_dash) {
  const float bad[] = {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity()};
  for (float value : bad) {
    ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(400), pixels(40)})
                          .with_debug_name("row"));
    auto result = progress_bar(h.context(), mk(row.ent(), 0), value,
                               ComponentConfig{}
                                   .with_size(ComponentSize{pixels(300),
                                                            pixels(28)})
                                   .with_debug_name("pb"),
                               ProgressBarLabelStyle::Percentage);
    h.layout_only();

    CHECK(result.as<float>() == 0.f);
    CHECK(h.find("progress_track") != nullptr);
    CHECK(h.find("progress_fill") == nullptr);
    UIComponent *label = h.find("progress_label");
    CHECK(label != nullptr);
    if (label) {
      auto ent = UICollectionHolder::getEntityForID(label->id);
      CHECK(ent->get<HasLabel>().label == "—");
    }
  }
}

// The Fraction label casts the raw value and max; same non-finite guard.
TEST(progress_bar_nonfinite_fraction_label_is_dash) {
  ImmTestHarness h;
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  progress_bar(h.context(), mk(row.ent(), 0),
               std::numeric_limits<float>::quiet_NaN(),
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(300), pixels(28)})
                   .with_debug_name("pb"),
               ProgressBarLabelStyle::Fraction, 0.f, 10.f);
  h.layout_only();

  UIComponent *label = h.find("progress_label");
  CHECK(label != nullptr);
  if (label) {
    auto ent = UICollectionHolder::getEntityForID(label->id);
    CHECK(ent->get<HasLabel>().label == "—");
  }
}

// Finite labels are unchanged by the non-finite guard.
TEST(progress_bar_finite_labels_unchanged) {
  ImmTestHarness h;
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  progress_bar(h.context(), mk(row.ent(), 0), 3.f,
               ComponentConfig{}
                   .with_size(ComponentSize{pixels(300), pixels(28)})
                   .with_debug_name("pb"),
               ProgressBarLabelStyle::Fraction, 0.f, 4.f);
  h.layout_only();

  UIComponent *label = h.find("progress_label");
  CHECK(label != nullptr);
  if (label) {
    auto ent = UICollectionHolder::getEntityForID(label->id);
    CHECK(ent->get<HasLabel>().label == "3/4");
  }
}

// Circular progress stores 0 for non-finite input instead of NaN state.
TEST(circular_progress_nonfinite_stores_zero) {
  const float bad[] = {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity()};
  for (float value : bad) {
    ImmTestHarness h;
    auto result = circular_progress(h.context(), mk(h.root(), 0), value,
                                    ComponentConfig{}.with_debug_name("cp"));
    h.layout_only();

    CHECK(result.as<float>() == 0.f);
    CHECK(result.ent().get<HasCircularProgressState>().value == 0.f);
  }
}

// Finite circular values still clamp into state as before.
TEST(circular_progress_finite_values_unchanged) {
  ImmTestHarness h;
  auto result = circular_progress(h.context(), mk(h.root(), 0), 1.5f,
                                  ComponentConfig{}.with_debug_name("cp"));
  h.layout_only();

  CHECK(result.as<float>() == 1.f);
  CHECK(result.ent().get<HasCircularProgressState>().value == 1.f);
}

int main() { return ui_test::run_registered_tests("progress_bar tests"); }
