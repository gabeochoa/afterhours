// tab_toggle_geometry_test.cpp
// R-168: floatinghotel's two bundled geometry complaints, re-verified with
// exact rects at origin/main. The tab half was already fixed (the d24 cases
// in downstream_gaps_test.cpp pass), so these pin its correct geometry; the
// toggle half was real -- the label and track siblings had a 0px gap between
// them -- and the first toggle case pins the 8px gap the fix adds.

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

// The tab strip sits at its panel's origin (not screen 0,0), takes the
// panel's width, and its tabs tile the bar exactly.
TEST(tab_container_exact_origin_width_and_tiling) {
  ImmTestHarness h;
  auto column = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(600), pixels(400)})
                        .with_flex_direction(FlexDirection::Column));
  div(h.context(), mk(column.ent(), 0),
      ComponentConfig{}.with_size(ComponentSize{pixels(600), pixels(120)}));
  auto panel =
      div(h.context(), mk(column.ent(), 1),
          ComponentConfig{}
              .with_size(ComponentSize{pixels(400), pixels(200)})
              .with_margin(Margin{.left = pixels(50)})  // force x != 0
              .with_debug_name("panel"));

  std::vector<std::string> labels{"One", "Two", "Three"};
  size_t active = 0;
  auto bar = tab_container(h.context(), mk(panel.ent(), 0), labels, active,
                           ComponentConfig{}
                               .with_size(ComponentSize{percent(1.0f),
                                                        pixels(48)})
                               .with_debug_name("bar"));
  h.layout_only();

  Rectangle p = rect_of(panel), b = rect_of(bar);
  CHECK_APPROX(b.x, 50.f);   // panel's margined origin, not the screen's
  CHECK_APPROX(b.y, 120.f);  // below the spacer row
  CHECK_APPROX(b.width, 400.f);
  CHECK_APPROX(b.height, 48.f);
  CHECK_APPROX(p.x, 50.f);
  CHECK_APPROX(p.width, 400.f);
  for (int i = 0; i < 3; ++i) {
    UIComponent *t = h.find("tab_" + std::to_string(i));
    CHECK(t != nullptr);
    if (!t)
      continue;
    // Exact thirds of the 400px bar, starting at the bar's origin.
    CHECK_APPROX(t->rect().x, 50.f + static_cast<float>(i) * (400.f / 3.f));
    CHECK_APPROX(t->rect().width, 400.f / 3.f);
  }
}

// The toggle's label and track are siblings with a real gap between them:
// total row width = label + 8px gap + 52px track, and the knob sits inside
// the track at pad=4 on both ends (20px knob, 24px travel).
TEST(toggle_switch_label_track_gap_and_knob_insets) {
  ImmTestHarness h;
  auto column = div(h.context(), mk(h.root(), 0),
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(300), pixels(300)})
                        .with_flex_direction(FlexDirection::Column));
  bool value = false;
  auto toggle =
      toggle_switch(h.context(), mk(column.ent(), 0), value,
                    ComponentConfig{}
                        .with_size(ComponentSize{pixels(300), pixels(40)})
                        .with_label("Enable")
                        .with_debug_name("tgl"));
  h.layout_only();

  Rectangle row = rect_of(toggle);
  UIComponent *label = h.find("toggle_label");
  UIComponent *track = h.find("toggle_track");
  UIComponent *knob = h.find("toggle_knob");
  CHECK(label && track && knob);
  if (!label || !track || !knob)
    return;

  CHECK_APPROX(row.width, 300.f);
  CHECK_APPROX(track->rect().width, 52.f);
  CHECK_APPROX(track->rect().height, 28.f);
  // 8px gap between the label sibling and the track sibling.
  CHECK_APPROX(track->rect().x - (label->rect().x + label->rect().width), 8.f);
  // Track is flush against the row's right edge.
  CHECK_APPROX(track->rect().x + track->rect().width, row.x + row.width);
  // Label + gap + track account for the whole row width.
  CHECK_APPROX(label->rect().width + 8.f + track->rect().width, row.width);
  // Knob inset by pad=4 from the track's leading edge (value off).
  CHECK_APPROX(knob->rect().x - track->rect().x, 4.f);
  CHECK_APPROX(knob->rect().width, 20.f);
  // Track vertically centered in the 40px row.
  CHECK_APPROX(track->rect().y - row.y, 6.f);
}

int main() {
  return ui_test::run_registered_tests("tab_toggle_geometry tests");
}
