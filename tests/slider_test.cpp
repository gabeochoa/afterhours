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

#include <cmath>
#include <limits>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

namespace {
// Builds one compact slider and returns its background (state) entity.
Entity &build_slider(ImmTestHarness &h, float &value) {
  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  slider(h.context(), mk(row.ent(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{percent(0.9f), pixels(36)})
             .with_debug_name("sl"),
         SliderHandleValueLabelPosition::None);
  h.layout_only();
  UIComponent *bg = h.find("slider_background");
  return AutoLayout::to_ent_static(bg->id);
}
} // namespace

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
  h.layout_only();

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
  h.layout_only();

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
    h.layout_only();
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

// A slider that is never interacted with reports "not changed" every frame.
// (The bug: changed_since was set true unconditionally, so the ElementResult
// bool was always true and `if (slider(...))` fired every frame.)
TEST(slider_reports_unchanged_without_interaction) {
  ImmTestHarness h;

  auto row = hstack(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(400), pixels(40)})
                        .with_debug_name("row"));
  float value = 0.5f;
  // First frame: creation may report a change; that's fine. Re-run a second
  // frame with no input and require it reports unchanged.
  slider(h.context(), mk(row.ent(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{percent(0.9f), pixels(36)})
             .with_debug_name("sl"),
         SliderHandleValueLabelPosition::None);
  h.layout_only();

  auto result = slider(h.context(), mk(row.ent(), 0), value,
                       ComponentConfig{}
                           .with_size(ComponentSize{percent(0.9f), pixels(36)})
                           .with_debug_name("sl"),
                       SliderHandleValueLabelPosition::None);
  h.layout_only();

  CHECK(!static_cast<bool>(result)); // no interaction => not changed
}

// A colour set on the slider reaches its parts. They used to take a theme
// usage unconditionally, which left custom_color populated and unread, so the
// track and handle could not be styled at all.
TEST(a_custom_colour_reaches_the_track_and_handle) {
  ImmTestHarness h;
  float value = 0.5f;
  const Color want{200, 30, 90, 255};

  slider(h.context(), mk(h.root(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{pixels(300), pixels(40)})
             .with_custom_background(want)
             .with_debug_name("styled"));
  h.layout_only();

  for (const char *part : {"slider_background", "slider_handle"}) {
    UIComponent *c = h.find(part);
    CHECK(c != nullptr);
    if (!c)
      continue;
    Entity &e = AutoLayout::to_ent_static(c->id);
    CHECK(e.has<HasColor>());
    if (e.has<HasColor>()) {
      const Color got = e.get<HasColor>().color();
      CHECK(got.r == want.r && got.g == want.g && got.b == want.b);
    }
  }
}

// Without a colour it still takes the theme's, or every unstyled slider would
// go transparent.
TEST(an_unstyled_slider_still_takes_the_theme) {
  ImmTestHarness h;
  float value = 0.5f;
  slider(h.context(), mk(h.root(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{pixels(300), pixels(40)})
             .with_debug_name("plain"));
  h.layout_only();

  UIComponent *bg = h.find("slider_background");
  CHECK(bg != nullptr);
  if (bg) {
    Entity &e = AutoLayout::to_ent_static(bg->id);
    CHECK(e.has<HasColor>());
    if (e.has<HasColor>())
      CHECK(e.get<HasColor>().color().a > 0);
  }
}

TEST(slider_thumb_stays_within_track_for_every_size_unit) {
  for (const auto width : {pixels(400), percent(.8f), w1280(600)}) {
    for (float value : {0.f, .6f, 1.f}) {
      ImmTestHarness h;
      auto control = slider(h.context(), mk(h.root(), 0), value,
          ComponentConfig{}.with_size({width, h720(44)}));
      h.layout_only();
      auto *track = h.find("slider_background");
      auto *thumb = h.find("slider_handle");
      CHECK(track && thumb);
      if (!track || !thumb) continue;
      CHECK(std::abs(track->rect().width - control.cmp().rect().width) < .05f);
      CHECK(std::abs(thumb->rect().width - track->rect().width * .25f) < .05f);
      CHECK(std::abs(thumb->rect().height - track->rect().height) < .05f);
      CHECK(std::abs(thumb->rect().x - track->rect().x -
          value * .75f * track->rect().width) < .05f);
      CHECK(thumb->rect().x + thumb->rect().width <= track->rect().x + track->rect().width + .05f);
    }
  }
}

// The percent label rounds; static_cast<int> truncated 0.296 to "29%".
TEST(slider_label_rounds_percent) {
  ImmTestHarness h;
  float value = 0.296f;
  slider(h.context(), mk(h.root(), 0), value,
         ComponentConfig{}
             .with_size(ComponentSize{pixels(400), pixels(36)})
             .with_label("Vol")
             .with_debug_name("sl"),
         SliderHandleValueLabelPosition::WithLabel);
  h.layout_only();

  UIComponent *label = h.find("slider_text");
  CHECK(label != nullptr);
  if (label) {
    auto ent = UICollectionHolder::getEntityForID(label->id);
    CHECK(ent->get<HasLabel>().label == "Vol: 30%");
  }
}

// Bad caller values sanitize on init and the handle stays in the track.
TEST(slider_sanitizes_bad_values) {
  const float bad[] = {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity(),
                       -std::numeric_limits<float>::infinity(), 1.5f, -0.5f};
  for (float v : bad) {
    ImmTestHarness h;
    float value = v;
    build_slider(h, value);
    CHECK(std::isfinite(value));
    CHECK(value >= 0.f && value <= 1.f);
    UIComponent *track = h.find("slider_background");
    UIComponent *handle = h.find("slider_handle");
    CHECK(track != nullptr && handle != nullptr);
    if (track && handle) {
      Rectangle tr = track->rect();
      Rectangle hr = handle->rect();
      CHECK(hr.x >= tr.x - 0.05f);
      CHECK(hr.x + hr.width <= tr.x + tr.width + 0.05f);
    }
  }
}

// WidgetPress on a focused slider must not fire the drag callback.
TEST(slider_keyboard_press_does_not_jump_to_mouse) {
  ImmTestHarness h;
  float value = 0.5f;
  Entity &bg = build_slider(h, value);

  h.context().set_focus(bg.id);
  h.context().mouse.pos = {10000.f, 0.f}; // mouse far right of the track
  h.context().last_action = TestInputAction::WidgetPress;

  HandleDrags<TestInputAction> drags;
  drags.context = &h.context();
  drags.for_each_with(bg, bg.get<UIComponent>(), bg.get<HasDragListener>(),
                      0.f);

  CHECK(bg.get<HasSliderState>().value == 0.5f);
}

// Pressing the handle's edge keeps the value; dragging moves relative.
TEST(slider_grab_keeps_grab_point) {
  ImmTestHarness h;
  float value = 0.5f;
  Entity &bg = build_slider(h, value);
  UIComponent *track = h.find("slider_background");
  UIComponent *handle = h.find("slider_handle");
  CHECK(track != nullptr && handle != nullptr);
  if (!track || !handle)
    return;
  const Rectangle tr = track->rect();
  const Rectangle hr = handle->rect();
  auto &listener = bg.get<HasDragListener>();

  // Press one pixel inside the handle's left edge.
  h.context().mouse.pos = {hr.x + 1.f, tr.y + tr.height * 0.5f};
  h.context().mouse.just_pressed = true;
  listener.cb(bg);
  CHECK(std::fabs(bg.get<HasSliderState>().value - 0.5f) < 0.01f);

  // Drag right by a tenth of the handle travel: value follows relatively.
  const float travel = tr.width - hr.width;
  h.context().mouse.pos = {hr.x + 1.f + travel * 0.1f, tr.y + tr.height * 0.5f};
  h.context().mouse.just_pressed = false;
  listener.cb(bg);
  CHECK(std::fabs(bg.get<HasSliderState>().value - 0.6f) < 0.01f);
}

// Clicking the track away from the handle centres the handle on the click.
TEST(slider_track_click_centres_handle) {
  ImmTestHarness h;
  float value = 0.f;
  Entity &bg = build_slider(h, value);
  UIComponent *track = h.find("slider_background");
  UIComponent *handle = h.find("slider_handle");
  CHECK(track != nullptr && handle != nullptr);
  if (!track || !handle)
    return;
  const Rectangle tr = track->rect();
  const float hw = handle->rect().width;

  // Click where value 0.25's handle centre belongs.
  const float x = tr.x + hw * 0.5f + 0.25f * (tr.width - hw);
  h.context().mouse.pos = {x, tr.y + tr.height * 0.5f};
  h.context().mouse.just_pressed = true;
  bg.get<HasDragListener>().cb(bg);
  CHECK(std::fabs(bg.get<HasSliderState>().value - 0.25f) < 0.01f);
}

int main() { return ui_test::run_registered_tests("slider tests"); }
