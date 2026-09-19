#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(input_gates_block_wheel_and_thumb_dragging) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto view = div(h.context(), mk(h.root(), 0), ComponentConfig{}
      .with_size({pixels(200), pixels(100)})
      .with_overflow(Overflow::Scroll, Axis::Y));
  auto foreground = div(h.context(), mk(h.root(), 1), ComponentConfig{}
      .with_size({pixels(200), pixels(100)})
      .with_absolute_position(0, 0).with_overflow(Overflow::Scroll, Axis::Y));
  h.layout_only();
  auto &entity = view.ent();
  auto &cmp = entity.get<UIComponent>();
  auto &scroll = entity.get<HasScrollView>();
  scroll.content_size = {400, 500};
  scroll.viewport_size = {200, 100};
  scroll.horizontal_enabled = true;
  const auto rect = cmp.rect();
  h.context().mouse.pos = {rect.x + 50, rect.y + 50};
  auto &front_scroll = foreground.ent().get<HasScrollView>();
  front_scroll.content_size = {400, 500};
  front_scroll.horizontal_enabled = true;
  h.context().add_input_gate("modal", [id = foreground.ent().id](EntityID candidate) {
    return candidate == id;
  });
  HandleScrollInput<ui_test::TestInputAction> wheel;
  wheel.context = &h.context();
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::input_injector::set_mouse_wheel(-1, -2);
  wheel.for_each_with(entity, cmp, scroll, 1.f / 60.f);
  CHECK_APPROX(scroll.scroll_target.x, 0);
  CHECK_APPROX(scroll.scroll_target.y, 0);
  wheel.for_each_with(foreground.ent(), foreground.ent().get<UIComponent>(),
                      front_scroll, 1.f / 60.f);
  CHECK(front_scroll.scroll_target.x > 0);
  CHECK(front_scroll.scroll_target.y > 0);
  h.context().remove_input_gate("modal");
  wheel.for_each_with(entity, cmp, scroll, 1.f / 60.f);
  CHECK(scroll.scroll_target.y > 0);
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
  scroll.scroll_offset = scroll.scroll_target = {0, 0};

  HandleScrollbarDrag<ui_test::TestInputAction> drag;
  drag.context = &h.context();
  const auto metrics = scrollbar_metrics(scroll, cmp.resolved_scaling_mode,
                                          h.context().screen_height);
  const auto geometry = scrollbar_geometry(scroll, rect, true,
                                            metrics.thickness, metrics.min_thumb);
  h.context().mouse.pos = {geometry.thumb.x + geometry.thumb.width / 2,
                           geometry.thumb.y + geometry.thumb.height / 2};
  h.context().mouse.just_pressed = true;
  h.context().mouse.left_down = true;
  h.context().add_input_gate("modal", [](EntityID) { return false; });
  drag.for_each_with(entity, cmp, scroll, 0);
  CHECK(!scroll.dragging_scrollbar);
  h.context().remove_input_gate("modal");
  drag.for_each_with(entity, cmp, scroll, 0);
  CHECK(scroll.dragging_scrollbar);
  h.context().mouse.just_pressed = false;
  h.context().mouse.pos.y += 30;
  h.context().add_input_gate("modal", [](EntityID) { return false; });
  drag.for_each_with(entity, cmp, scroll, 0);
  CHECK(!scroll.dragging_scrollbar);
  CHECK_APPROX(scroll.scroll_offset.y, 0);
}

int main() { return ui_test::run_registered_tests("scroll input gates"); }
