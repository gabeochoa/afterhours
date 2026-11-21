// e2e_window_size_test.cpp - 'expect_window_size w h' asserts the current
// resolution singleton, retrying while a preceding 'resize' settles.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <cstdio>

using namespace afterhours;
using namespace afterhours::testing;

namespace {

struct ResolutionFixture {
  EntityCollection coll;
  Entity &entity = coll.createEntity();
  Entity &singleton = coll.createEntity();

  explicit ResolutionFixture(window_manager::Resolution start = {800, 600}) {
    EntityHelper::set_default_collection(&coll);
    coll.merge_entity_arrays();
    singleton.addComponent<window_manager::ProvidesCurrentResolution>(start);
    EntityHelper::registerSingleton<
        window_manager::ProvidesCurrentResolution>(singleton);
  }
  ~ResolutionFixture() { EntityHelper::set_default_collection(nullptr); }

  void set_size(int w, int h) {
    auto &pcr = singleton.get<window_manager::ProvidesCurrentResolution>();
    pcr.current_resolution = {w, h};
  }
};

PendingE2ECommand make_cmd(std::vector<std::string> args) {
  PendingE2ECommand cmd;
  cmd.name = "expect_window_size";
  cmd.args = std::move(args);
  return cmd;
}

}  // namespace

TEST(passes_when_current_size_matches) {
  ResolutionFixture f;
  HandleExpectWindowSizeCommand handler;
  auto cmd = make_cmd({"800", "600"});
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(retries_until_a_resize_settles_then_passes) {
  ResolutionFixture f;
  HandleExpectWindowSizeCommand handler;
  auto cmd = make_cmd({"1024", "768"});
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_retry());
  CHECK(cmd.last_mismatch.find("800x600") != std::string::npos);
  cmd.reset_retry();
  f.set_size(1024, 768);  // 'resize' lands a frame later
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(never_matching_size_fails_at_timeout_with_mismatch) {
  ResolutionFixture f;
  HandleExpectWindowSizeCommand handler;
  auto cmd = make_cmd({"1024", "768"});
  E2ECommandCleanupSystem cleanup;
  for (int i = 0; i <= PendingE2ECommand::MAX_FRAMES; i++) {
    handler.for_each_with(f.entity, cmd, 0.f);
    cleanup.for_each_with(f.entity, cmd, 0.f);
  }
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message.find("800x600") != std::string::npos);
}

TEST(missing_args_and_bad_dimensions_fail_immediately) {
  ResolutionFixture f;
  HandleExpectWindowSizeCommand handler;
  auto no_args = make_cmd({});
  handler.for_each_with(f.entity, no_args, 0.f);
  CHECK(no_args.is_consumed() && !no_args.succeeded());

  auto zero = make_cmd({"0", "600"});
  handler.for_each_with(f.entity, zero, 0.f);
  CHECK(zero.is_consumed() && !zero.succeeded());
}

TEST(script_parses_expect_window_size_as_two_args) {
  const auto path = std::filesystem::temp_directory_path() /
      "afterhours-window-size.e2e";
  {
    std::ofstream out(path);
    out << "expect_window_size 1024 768\n";
  }
  const auto cmds = parse_script(path.string());
  CHECK(cmds.size() == 1);
  CHECK(cmds[0].name == "expect_window_size");
  CHECK(cmds[0].args.size() == 2 && cmds[0].args[0] == "1024" &&
        cmds[0].args[1] == "768");
  std::filesystem::remove(path);
}

int main() { return ui_test::run_registered_tests("E2E expect_window_size"); }
