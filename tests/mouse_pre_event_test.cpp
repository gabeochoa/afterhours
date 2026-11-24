#include "ui_test_harness.h"

#include <cmath>
#include <limits>

using namespace afterhours;
using namespace afterhours::ui;

TEST(pre_event_mouse_pos_is_finite_and_off_screen) {
  UIContext<ui_test::TestInputAction> ctx;
  const auto pos = ctx.mouse.pos;
  ui_test::check(std::isfinite(pos.x) && std::isfinite(pos.y),
                 "pre-event mouse.pos is finite", __FILE__, __LINE__);
  ui_test::check(!is_mouse_inside(pos, RectangleType{0.f, 0.f, 800.f, 600.f}),
                 "pre-event mouse.pos hits nothing", __FILE__, __LINE__);
}

TEST(a_non_finite_mouse_pos_never_hits) {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ui_test::check(
      !is_mouse_inside(input::MousePosition{nan, nan},
                       RectangleType{0.f, 0.f, 800.f, 600.f}),
      "NaN mouse.pos is never inside", __FILE__, __LINE__);
}

int main() { return ui_test::run_registered_tests("mouse_pre_event"); }
