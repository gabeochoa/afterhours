#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

namespace {
void wheel_over(ui_test::ImmTestHarness &h, Entity &view, Vector2Type at, float dx, float dy) {
  HandleScrollInput<ui_test::TestInputAction> wheel;
  wheel.context = &h.context();
  h.context().mouse.pos = at;
  testing::input_injector::set_mouse_wheel(dx, dy);
  wheel.for_each_with(view, view.get<UIComponent>(), view.get<HasScrollView>(), 1.f / 60.f);
}
} // namespace

TEST(wheel_reaches_a_scroll_view_where_its_scrolled_parent_draws_it) {
  ui_test::ImmTestHarness h;
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  h.begin_frame();
  auto outer = div(h.context(), mk(h.root(), 0), ComponentConfig{}
      .with_size({pixels(200), pixels(200)})
      .with_absolute_position(0, 0)
      .with_overflow(Overflow::Scroll, Axis::Y));
  div(h.context(), mk(outer.ent(), 0), ComponentConfig{}.with_size({pixels(200), pixels(100)}));
  auto inner = div(h.context(), mk(outer.ent(), 1), ComponentConfig{}
      .with_size({pixels(200), pixels(100)})
      .with_overflow(Overflow::Scroll, Axis::Y));
  h.layout_only();
  outer.ent().get<HasScrollView>().scroll_offset = {0, 48};
  auto &scroll = inner.ent().get<HasScrollView>();
  scroll.content_size = {200, 500};

  wheel_over(h, inner.ent(), {50, 60}, 0, -1);
  CHECK(scroll.scroll_target.y > 0);

  scroll.scroll_offset = scroll.scroll_target = {0, 0};
  wheel_over(h, inner.ent(), {50, 180}, 0, -1);
  CHECK_APPROX(scroll.scroll_target.y, 0);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
}

TEST(shift_wheel_scrolls_a_horizontal_view) {
  ui_test::ImmTestHarness h;
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  h.begin_frame();
  auto strip = div(h.context(), mk(h.root(), 0), ComponentConfig{}
      .with_size({pixels(200), pixels(40)})
      .with_absolute_position(0, 0)
      .with_overflow(Overflow::Scroll, Axis::X));
  h.layout_only();
  auto &scroll = strip.ent().get<HasScrollView>();
  scroll.content_size = {800, 40};

  wheel_over(h, strip.ent(), {50, 20}, 0, -1);
  CHECK_APPROX(scroll.scroll_target.x, 0);

  testing::input_injector::set_key_held(keys::LEFT_SHIFT);
  wheel_over(h, strip.ent(), {50, 20}, 0, -1);
  CHECK(scroll.scroll_target.x > 0);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
}

int main() { return ui_test::run_registered_tests("wheel input"); }
