// build_geometry_test.cpp
// Geometry on the build frame (hanabi #286): rect() during build is last
// frame's rect -- 0x0 on the first frame -- so a widget cannot hit-test a
// press against itself until a frame later. rect_now()/point_in() lay out
// on demand and must agree with the layout pass that runs after build.

#include "ui_test_harness.h"
#include <afterhours/src/plugins/ui/imm_layout.h>
#include <afterhours/src/plugins/ui/systems.h>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;

static bool rect_eq(RectangleType a, RectangleType b) {
  return std::fabs(a.x - b.x) < 1.5f && std::fabs(a.y - b.y) < 1.5f &&
         std::fabs(a.width - b.width) < 1.5f &&
         std::fabs(a.height - b.height) < 1.5f;
}

// The hanabi rail: absolute inside an offset pane. The configured translate
// (384,6) is in the pane's frame; the window-space answer is (664,73).
TEST(first_frame_absolute_rail) {
  ImmTestHarness h;
  auto pane = div(h.context(), mk(h.root(), 100),
                  ComponentConfig{}
                      .with_size({pixels(400), pixels(583)})
                      .with_absolute_position(280.f, 67.f));
  auto rail = div(h.context(), mk(pane.ent(), 7400),
                  ComponentConfig{}
                      .with_size({pixels(10), pixels(577)})
                      .with_absolute_position(384.f, 6.f));

  auto stale = rail.cmp().rect();
  CHECK_APPROX(stale.width, 0.f); // never laid out: the reported trap

  auto now = rail.rect_now();
  CHECK_APPROX(now.x, 664.f);
  CHECK_APPROX(now.y, 73.f);
  CHECK_APPROX(now.width, 10.f);
  CHECK_APPROX(now.height, 577.f);
  CHECK(h.context().point_in(rail.id(), {669.f, 100.f}));
  CHECK(!h.context().point_in(rail.id(), {100.f, 100.f}));
  CHECK(rail.point_in({669.f, 100.f}));

  h.layout_only(); // the pass that used to be the first correct answer
  CHECK(rect_eq(now, rail.cmp().rect()));
}

// A flow widget too: rect_now equals the post-build layout, not just the
// absolute fast path.
TEST(flow_widget_same_frame) {
  ImmTestHarness h;
  auto pane = div(h.context(), mk(h.root(), 100),
                  ComponentConfig{}
                      .with_size({pixels(300), pixels(200)})
                      .with_flex_direction(FlexDirection::Column));
  div(h.context(), mk(pane.ent(), 0),
      ComponentConfig{}.with_size({pixels(300), pixels(50)}));
  auto body = div(h.context(), mk(pane.ent(), 1),
                  ComponentConfig{}.with_size({pixels(300), pixels(80)}));

  auto now = body.rect_now();
  CHECK_APPROX(now.y, 50.f);
  CHECK_APPROX(now.height, 80.f);

  h.layout_only();
  CHECK(rect_eq(now, body.cmp().rect()));
}

// After a resize/rebuild, the same-frame answer tracks the new geometry in
// the same call, rather than reporting the previous frame's rect.
TEST(resized_widget_same_frame) {
  ImmTestHarness h;
  auto build = [&](float pane_w, float rail_x) {
    h.begin_frame();
    auto pane = div(h.context(), mk(h.root(), 100),
                    ComponentConfig{}
                        .with_size({pixels(pane_w), pixels(583)})
                        .with_absolute_position(280.f, 67.f));
    return div(h.context(), mk(pane.ent(), 7400),
               ComponentConfig{}
                   .with_size({pixels(10), pixels(577)})
                   .with_absolute_position(rail_x, 6.f));
  };

  auto rail1 = build(400.f, 384.f);
  auto f1 = rail1.rect_now();
  h.layout_only();
  CHECK(rect_eq(f1, rail1.cmp().rect()));

  auto rail2 = build(500.f, 484.f); // pane widened, rail re-anchored
  auto f2 = rail2.rect_now();
  CHECK_APPROX(f2.x, 764.f);
  h.layout_only();
  CHECK(rect_eq(f2, rail2.cmp().rect()));
}

int main() { return ui_test::run_registered_tests("build_geometry"); }
