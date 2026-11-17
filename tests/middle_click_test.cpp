#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;

TEST(an_injected_middle_press_reaches_the_backend_read) {
  testing::test_input::detail::test_mode = true;
  testing::input_injector::reset_all();
  testing::test_input::set_mouse_position(10, 10);
  CHECK(!input::is_mouse_button_down(2));
  testing::input_injector::detail::mouse.middle_down = true;
  CHECK(input::is_mouse_button_down(2));
  CHECK(!input::is_mouse_button_down(0));
  testing::input_injector::reset_all();
  testing::test_input::detail::test_mode = false;
}

TEST(is_middle_click_answers_for_the_hovered_subtree_on_release) {
  ui_test::ImmTestHarness h;
  h.begin_frame();
  auto tab = div(h.context(), mk(h.root(), 0),
                 ComponentConfig{}.with_size({pixels(100), pixels(30)}));
  tab.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto close = div(h.context(), mk(tab.ent(), 0),
                   ComponentConfig{}.with_size({pixels(20), pixels(20)}));
  close.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  auto other = div(h.context(), mk(h.root(), 1),
                   ComponentConfig{}.with_size({pixels(100), pixels(30)}));
  other.ent().addComponentIfMissing<HasClickListener>([](Entity &) {});
  h.layout_only();

  h.context().prev_hot_id = close.ent().id;
  CHECK(!h.context().is_middle_click(tab.ent().id));
  h.context().mouse.middle_just_released = true;
  CHECK(h.context().is_middle_click(tab.ent().id));
  CHECK(!h.context().is_middle_click(other.ent().id));
  CHECK(!h.context().is_right_click(tab.ent().id));
}

int main() { return ui_test::run_registered_tests("middle click"); }
