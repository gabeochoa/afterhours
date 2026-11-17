#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

struct GestureFixture {
  ui_test::ImmTestHarness h;
  GestureFixture() {
    test_input::reset_all();
    h.begin_frame();
    ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
                 ui::imm::ComponentConfig{}
                     .with_size({ui::pixels(200), ui::pixels(40)})
                     .with_absolute_position(100, 50)
                     .with_debug_name("rail"));
    h.layout_only();
  }
  ~GestureFixture() { test_input::reset_all(); }
  PendingE2ECommand run(std::string name, std::vector<std::string> args) {
    PendingE2ECommand cmd;
    cmd.name = std::move(name);
    cmd.args = std::move(args);
    ui_commands::HandlePointerUICommand{}.for_each_with(h.root(), cmd, 0.f);
    return cmd;
  }
};

TEST(mouse_down_ui_presses_and_holds_on_the_element) {
  GestureFixture f;
  auto cmd = f.run("mouse_down_ui", {"rail", "10", "5"});
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 110.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 55.f);
  CHECK(inj::detail::mouse.left_down);
  test_input::reset_frame();
  test_input::reset_frame();
  CHECK(inj::detail::mouse.left_down);
}

TEST(mouse_move_ui_aims_at_the_centre_by_default) {
  GestureFixture f;
  auto cmd = f.run("mouse_move_ui", {"rail"});
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 200.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 70.f);
  CHECK(!inj::detail::mouse.left_down);
}

TEST(scroll_wheel_ui_moves_there_then_scrolls) {
  GestureFixture f;
  auto cmd = f.run("scroll_wheel_ui", {"rail", "0", "-3"});
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 200.f);
  CHECK_APPROX(inj::consume_wheel().y, -3.f);
}

TEST(a_missing_element_fails) {
  GestureFixture f;
  auto cmd = f.run("mouse_down_ui", {"nope"});
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message == "UI component not found: nope");
  CHECK(!inj::detail::mouse.left_down);
}

int main() { return ui_test::run_registered_tests("E2E element gestures"); }
