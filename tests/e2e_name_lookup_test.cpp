#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>
#include <afterhours/src/plugins/ui/text_input/state.h>
#include <afterhours/src/plugins/ui/text_input/text_area_state.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

using ui_test::ImmTestHarness;

static PendingE2ECommand pending_for(std::string name,
                                     std::vector<std::string> args) {
  PendingE2ECommand pending;
  pending.name = std::move(name);
  pending.args = std::move(args);
  return pending;
}

// Two panels, each holding a row named "row" labelled "Same" with a
// different width, so scope picks the element and the click point proves it.
// Left row: (0,0)-(100,40), centre (50,20). Right row: (0,120)-(220,160),
// centre (110,140).
static void build_two_panels(ImmTestHarness &h) {
  h.begin_frame();
  auto left = ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
                           ui::imm::ComponentConfig{}
                               .with_size({ui::pixels(300), ui::pixels(100)})
                               .with_debug_name("left_panel"));
  ui::imm::div(h.context(), ui::imm::mk(left.ent(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(100), ui::pixels(40)})
                   .with_label("Same")
                   .with_debug_name("row"));
  auto right = ui::imm::div(h.context(), ui::imm::mk(h.root(), 1),
                            ui::imm::ComponentConfig{}
                                .with_size({ui::pixels(300), ui::pixels(100)})
                                .with_absolute_position(0, 120)
                                .with_debug_name("right_panel"));
  ui::imm::div(h.context(), ui::imm::mk(right.ent(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(220), ui::pixels(40)})
                   .with_label("Same")
                   .with_debug_name("row"));
  h.layout_only();
}

TEST(click_ui_unscoped_single_match_unchanged) {
  ImmTestHarness h;
  h.begin_frame();
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(200), ui::pixels(40)})
                   .with_absolute_position(100, 50)
                   .with_debug_name("rail"));
  h.layout_only();
  test_input::reset_all();
  auto cmd = pending_for("click_ui", {"rail"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 200.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 70.f);
  test_input::reset_all();
}

TEST(click_ui_ambiguous_fails_loudly_with_candidates) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto cmd = pending_for("click_ui", {"row"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("2 elements match") != std::string::npos);
  CHECK(cmd.error_message.find("'row'") != std::string::npos);
  CHECK(cmd.error_message.find("under=<scope>") != std::string::npos);
  CHECK(cmd.error_message.find("candidates") != std::string::npos);
  CHECK(cmd.error_message.find("left_panel") != std::string::npos);
  CHECK(cmd.error_message.find("right_panel") != std::string::npos);
  // Nothing was clicked.
  CHECK(!inj::detail::mouse.left_down);
  test_input::reset_all();
}

TEST(click_ui_under_scope_picks_the_in_scope_row) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto left = pending_for("click_ui", {"row", "under=left_panel"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), left, 0.f);
  CHECK(left.is_consumed() && left.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 50.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 20.f);
  test_input::reset_all();
  auto right = pending_for("click_ui", {"row", "under=right_panel"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), right, 0.f);
  CHECK(right.is_consumed() && right.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 110.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 140.f);
  test_input::reset_all();
}

TEST(click_ui_in_alias_still_works) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto cmd = pending_for("click_ui", {"row", "in=right_panel"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 110.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 140.f);
  test_input::reset_all();
}

TEST(click_ui_unknown_scope_fails_like_missing_target) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto cmd = pending_for("click_ui", {"row", "under=no_such_panel"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("scope 'no_such_panel' not found") != std::string::npos);
  test_input::reset_all();
}

TEST(ambiguous_scope_name_fails) {
  ImmTestHarness h;
  h.begin_frame();
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(300), ui::pixels(100)})
                   .with_debug_name("pool"));
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 1),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(300), ui::pixels(100)})
                   .with_absolute_position(0, 120)
                   .with_debug_name("pool"));
  h.layout_only();
  auto cmd = pending_for("click_ui", {"pool"});
  ui_commands::HandleClickUICommand<ui_test::TestInputAction>{}.for_each_with(h.root(), cmd, 0.f);
  // Even the scope lookup itself is first-match today; targeting 'pool'
  // directly must fail rather than pick one.
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("2 elements match") != std::string::npos);
}

TEST(click_text_scoped_and_ambiguous) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto amb = pending_for("click_text", {"Same"});
  ui_commands::HandleClickTextCommand<ui_test::TestInputAction>{}.for_each_with(h.root(), amb, 0.f);
  CHECK(amb.is_consumed() && !amb.succeeded());
  CHECK(amb.error_message.find("2 elements match") != std::string::npos);
  auto scoped = pending_for("click_text", {"Same", "under=left_panel"});
  ui_commands::HandleClickTextCommand<ui_test::TestInputAction>{}.for_each_with(h.root(), scoped, 0.f);
  CHECK(scoped.is_consumed() && scoped.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 50.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 20.f);
  test_input::reset_all();
}

TEST(expect_point_in_with_under_scope) {
  ImmTestHarness h;
  build_two_panels(h);
  // (50,20) is inside the LEFT row; only visible in that scope.
  auto good = pending_for("expect_point_in", {"row", "50", "20", "under=left_panel"});
  ui_commands::HandleExpectPointInCommand{}.for_each_with(h.root(), good, 0.f);
  CHECK(good.is_consumed() && good.succeeded());
  auto wrong = pending_for("expect_point_in", {"row", "50", "20", "under=right_panel"});
  ui_commands::HandleExpectPointInCommand{}.for_each_with(h.root(), wrong, 0.f);
  CHECK(wrong.is_retry());
  auto amb = pending_for("expect_point_in", {"row", "50", "20"});
  ui_commands::HandleExpectPointInCommand{}.for_each_with(h.root(), amb, 0.f);
  CHECK(amb.is_consumed() && !amb.succeeded());
  CHECK(amb.error_message.find("2 elements match") != std::string::npos);
}

TEST(mouse_move_ui_with_under_scope) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto cmd = pending_for("mouse_move_ui", {"row", "under=right_panel"});
  ui_commands::HandlePointerUICommand{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK_APPROX(inj::detail::mouse.pos.x, 110.f);
  CHECK_APPROX(inj::detail::mouse.pos.y, 140.f);
  test_input::reset_all();
  auto amb = pending_for("mouse_move_ui", {"row"});
  ui_commands::HandlePointerUICommand{}.for_each_with(h.root(), amb, 0.f);
  CHECK(amb.is_consumed() && !amb.succeeded());
  CHECK(amb.error_message.find("2 elements match") != std::string::npos);
  test_input::reset_all();
}

TEST(double_click_ui_ambiguous_fails) {
  ImmTestHarness h;
  build_two_panels(h);
  test_input::reset_all();
  auto cmd = pending_for("double_click_ui", {"row"});
  ui_commands::HandleMultiClickUICommand<ui_test::TestInputAction, 2>{}.for_each_with(h.root(), cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("2 elements match") != std::string::npos);
  test_input::reset_all();
}

TEST(single_match_unchanged_across_assertions) {
  ImmTestHarness h;
  h.begin_frame();
  ui::imm::div(h.context(), ui::imm::mk(h.root(), 0),
               ui::imm::ComponentConfig{}
                   .with_size({ui::pixels(200), ui::pixels(40)})
                   .with_label("Only")
                   .with_debug_name("only"));
  h.layout_only();
  auto p = pending_for("expect_point_in", {"only", "100", "20"});
  ui_commands::HandleExpectPointInCommand{}.for_each_with(h.root(), p, 0.f);
  CHECK(p.is_consumed() && p.succeeded());
}

int main() { return ui_test::run_registered_tests("E2E name lookup"); }
