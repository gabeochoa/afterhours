#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/pending_command.h>

#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using afterhours::testing::PendingE2ECommand;
using afterhours::testing::ui_commands::HandleExpectPointInCommand;

static PendingE2ECommand &make_cmd(ImmTestHarness &h, Entity *&holder,
                                   const std::vector<std::string> &args) {
  holder = &h.coll.createEntity();
  auto &cmd = holder->addComponent<PendingE2ECommand>();
  cmd.name = "expect_point_in";
  cmd.args = args;
  return cmd;
}

static void build_panel(ImmTestHarness &h) {
  div(h.context(), mk(h.root(), 0),
      ComponentConfig{}
          .with_size(ComponentSize{pixels(200), pixels(100)})
          .with_debug_name("panel"));
  h.layout_only();
}

TEST(point_inside_consumes) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "50", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(point_at_centre_consumes) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "100", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(point_outside_retries_with_rect) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "250", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(!cmd.is_consumed());
  CHECK(cmd.is_retry());
  CHECK(cmd.last_mismatch.find("panel") != std::string::npos);
  CHECK(cmd.last_mismatch.find("250") != std::string::npos);
  CHECK(cmd.last_mismatch.find("200x100") != std::string::npos);
}

TEST(point_below_retries) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "50", "150"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(!cmd.is_consumed() && cmd.is_retry());
}

TEST(missing_element_retries) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"nope", "50", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(!cmd.is_consumed() && cmd.is_retry());
}

TEST(percent_coords_resolve_against_screen) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "10%", "10%"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(missing_args_fails) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("expect_point_in requires") !=
        std::string::npos);
}

TEST(invalid_coordinate_fails) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "left", "50"});
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("invalid coordinate") != std::string::npos);
}

TEST(other_command_ignored) {
  ImmTestHarness h;
  build_panel(h);
  Entity *holder = nullptr;
  auto &cmd = make_cmd(h, holder, {"panel", "50", "50"});
  cmd.name = "assert_ui";
  HandleExpectPointInCommand handler;
  handler.for_each_with(*holder, cmd, 0.f);
  CHECK(!cmd.is_consumed() && !cmd.is_retry());
}

int main() { return ui_test::run_registered_tests("point_in_element tests"); }
