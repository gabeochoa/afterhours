// slider_test.cpp
// Regression tests for the imm::slider widget.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/slider_test.cpp -o /tmp/t && /tmp/t
//
// Covers the handle-position bug on percent-sized tracks: the handle left
// offset was value * 0.75 * track_val, but the handle is a child of the track
// so a percent margin already resolves against the track width — the extra
// track_val double-applied and pinned the knob near the start (an 80% slider
// showed the handle at ~30%).

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

// On a percent-width track, a high value places the handle in the right half.
TEST(slider_percent_track_handle_high_value) {
  ImmTestHarness h;

  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  float value = 0.8f;
  slider(h.context(), mk(row.ent(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{percent(0.9f), pixels(36)})
             .with_debug_name("sl"),
         SliderHandleValueLabelPosition::None);
  h.layout_and_render();

  UIComponent *track = h.find("slider_background");
  UIComponent *handle = h.find("slider_handle");
  CHECK(track != nullptr);
  CHECK(handle != nullptr);
  if (track && handle) {
    Rectangle tr = track->rect();
    Rectangle hr = handle->rect();
    float handle_center = hr.x + hr.width * 0.5f;
    float track_mid = tr.x + tr.width * 0.5f;
    // 80% value -> knob in the right portion of the track (was ~30% pre-fix).
    CHECK(handle_center > track_mid);
    // Handle stays inside the track horizontally.
    CHECK(hr.x >= tr.x - 1.f);
    CHECK(hr.x + hr.width <= tr.x + tr.width + 1.f);
  }
}

// A low value keeps the handle in the left half of a percent-width track.
TEST(slider_percent_track_handle_low_value) {
  ImmTestHarness h;

  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  float value = 0.1f;
  slider(h.context(), mk(row.ent(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{percent(0.9f), pixels(36)})
             .with_debug_name("sl"),
         SliderHandleValueLabelPosition::None);
  h.layout_and_render();

  UIComponent *track = h.find("slider_background");
  UIComponent *handle = h.find("slider_handle");
  CHECK(track != nullptr);
  CHECK(handle != nullptr);
  if (track && handle) {
    Rectangle tr = track->rect();
    Rectangle hr = handle->rect();
    float handle_center = hr.x + hr.width * 0.5f;
    float track_mid = tr.x + tr.width * 0.5f;
    CHECK(handle_center < track_mid);
  }
}

// A higher value moves the handle strictly right of a lower value.
TEST(slider_handle_monotonic_in_value) {
  auto center_for = [](float value) -> float {
    ImmTestHarness h;
    auto row = hstack(h.context(), mk(h.root(), 0),
                      ComponentConfig{}
                          .with_size(ComponentSize{pixels(400), pixels(40)})
                          .with_debug_name("row"));
    slider(h.context(), mk(row.ent(), 0), value,
           ComponentConfig{}
               .with_size(ComponentSize{percent(0.9f), pixels(36)})
               .with_debug_name("sl"),
           SliderHandleValueLabelPosition::None);
    h.layout_and_render();
    UIComponent *handle = h.find("slider_handle");
    if (!handle)
      return -1.f;
    Rectangle hr = handle->rect();
    return hr.x + hr.width * 0.5f;
  };

  float low = center_for(0.2f);
  float high = center_for(0.7f);
  CHECK(low >= 0.f);
  CHECK(high >= 0.f);
  CHECK(high > low);
}

int main() { return ui_test::run_registered_tests("slider tests"); }
