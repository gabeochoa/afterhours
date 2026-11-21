#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
void wheel_over(ui_test::ImmTestHarness &h, Entity &view, float dx, float dy,
                WheelDeltaKind kind) {
  HandleScrollInput<ui_test::TestInputAction> wheel;
  wheel.context = &h.context();
  h.context().mouse.pos = {50, 50};
  testing::input_injector::set_mouse_wheel(dx, dy, kind);
  wheel.for_each_with(view, view.get<UIComponent>(),
                      view.get<HasScrollView>(), 1.f / 60.f);
}

Entity &make_view(ui_test::ImmTestHarness &h) {
  auto view = div(h.context(), mk(h.root(), 0),
                  ComponentConfig{}
                      .with_size({pixels(200), pixels(200)})
                      .with_absolute_position(0, 0)
                      .with_overflow(Overflow::Scroll, Axis::Y));
  h.layout_only();
  auto &scroll = view.ent().get<HasScrollView>();
  scroll.content_size = {200, 1000};
  return view.ent();
}
}

TEST(precise_and_notched_deltas_use_their_own_speed) {
  ui_test::ImmTestHarness h;
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  h.begin_frame();
  Entity &view = make_view(h);
  auto &scroll = view.get<HasScrollView>();
  scroll.scroll_speed = 20.f;
  scroll.scroll_speed_precise = 2.f;

  wheel_over(h, view, 0, -1, WheelDeltaKind::Notched);
  CHECK_APPROX(scroll.scroll_target.y, 20.f);

  scroll.scroll_offset = scroll.scroll_target = {0, 0};
  wheel_over(h, view, 0, -10, WheelDeltaKind::Precise);
  CHECK_APPROX(scroll.scroll_target.y, 20.f);

  wheel_over(h, view, 0, 0, WheelDeltaKind::Precise);
  CHECK_APPROX(scroll.scroll_offset.y, 20.f);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
}

TEST(precise_speed_defaults_to_todays_feel) {
  ui_test::ImmTestHarness h;
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  h.begin_frame();
  Entity &view = make_view(h);
  auto &scroll = view.get<HasScrollView>();
  CHECK_APPROX(scroll.scroll_speed_precise, scroll.scroll_speed);

  wheel_over(h, view, 0, -1, WheelDeltaKind::Precise);
  CHECK_APPROX(scroll.scroll_target.y, 20.f);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
}

int main() { return ui_test::run_registered_tests("wheel delta kind"); }
