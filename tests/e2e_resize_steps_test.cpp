// e2e_resize_steps_test.cpp
// resize_steps applies one intermediate size per frame through the resize
// path, and wait_resized only passes once the resolution singleton and the
// backend drawable both read the target and a frame has been presented at
// that size. Headless: the no-backend drawable tracks the last requested
// size, and a presented frame is a VisibleTextRegistry generation bump --
// what the UI render pass does once per frame in test mode.

#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace afterhours;
using namespace afterhours::testing;

namespace {

struct ResizeFixture {
  EntityCollection coll;
  Entity &entity = coll.createEntity();
  Entity &singleton = coll.createEntity();

  explicit ResizeFixture(window_manager::Resolution start = {800, 600}) {
    EntityHelper::set_default_collection(&coll);
    coll.merge_entity_arrays();
    singleton.addComponent<window_manager::ProvidesCurrentResolution>(start);
    EntityHelper::registerSingleton<
        window_manager::ProvidesCurrentResolution>(singleton);
    // The no-backend drawable starts wherever a previous test left it.
    window_manager::set_window_size(start.width, start.height);
    resize_steps_detail::reset();
    VisibleTextRegistry::instance().clear();
  }
  ~ResizeFixture() { EntityHelper::set_default_collection(nullptr); }

  window_manager::Resolution resolution() const {
    return singleton.get<window_manager::ProvidesCurrentResolution>()
        .current_resolution;
  }
  window_manager::Resolution drawable() const {
    return window_manager::fetch_raw_window_size();
  }
};

}  // namespace

TEST(resize_steps_applies_one_intermediate_size_per_frame) {
  ResizeFixture f;
  HandleResizeStepsCommand handler;
  PendingE2ECommand cmd;
  cmd.name = "resize_steps";
  cmd.args = {"1000", "800", "4"};

  const window_manager::Resolution expected[4] = {
      {850, 650}, {900, 700}, {950, 750}, {1000, 800}};
  for (int i = 0; i < 4; i++) {
    handler.for_each_with(f.entity, cmd, 0.f);
    CHECK(f.resolution() == expected[i]);
    CHECK(f.drawable() == expected[i]);
    if (i < 3) {
      CHECK(cmd.is_retry());
      cmd.reset_retry();
    }
  }
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(wait_resized_waits_for_a_presented_frame_at_the_target) {
  ResizeFixture f;
  apply_e2e_resize(1000, 800);
  HandleWaitResizedCommand handler;
  PendingE2ECommand cmd;
  cmd.name = "wait_resized";
  cmd.args = {"1000", "800"};

  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_retry());
  CHECK(cmd.last_mismatch.find("no frame presented") != std::string::npos);
  cmd.reset_retry();
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_retry());  // still no render pass since the size landed
  cmd.reset_retry();
  VisibleTextRegistry::instance().clear();  // one presented frame
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(wait_resized_reports_resolution_then_drawable_mismatches) {
  ResizeFixture f;  // at 800x600, singleton and drawable alike
  HandleWaitResizedCommand handler;
  PendingE2ECommand cmd;
  cmd.name = "wait_resized";
  cmd.args = {"640", "480"};

  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.last_mismatch.find("resolution is 800x600") != std::string::npos);
  cmd.reset_retry();

  // Singleton moved by hand; the drawable has not caught up yet.
  f.singleton.get<window_manager::ProvidesCurrentResolution>()
      .current_resolution = {640, 480};
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.last_mismatch.find("drawable is 800x600") != std::string::npos);
  cmd.reset_retry();

  apply_e2e_resize(640, 480);
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_retry());
  cmd.reset_retry();
  VisibleTextRegistry::instance().clear();
  handler.for_each_with(f.entity, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
}

TEST(resize_commands_fail_on_bad_arguments) {
  ResizeFixture f;
  HandleResizeStepsCommand steps_handler;
  PendingE2ECommand steps;
  steps.name = "resize_steps";
  steps.args = {"1000", "800", "0"};
  steps_handler.for_each_with(f.entity, steps, 0.f);
  CHECK(steps.is_consumed() && !steps.succeeded());
  CHECK(steps.error_message.find("invalid target") != std::string::npos);

  HandleWaitResizedCommand wait_handler;
  PendingE2ECommand wait_cmd;
  wait_cmd.name = "wait_resized";
  wait_cmd.args = {"1000"};
  wait_handler.for_each_with(f.entity, wait_cmd, 0.f);
  CHECK(wait_cmd.is_consumed() && !wait_cmd.succeeded());
}

TEST(resize_scripts_parse_and_run_end_to_end) {
  ResizeFixture f;
  const auto path = std::filesystem::temp_directory_path() /
                    "afterhours_e2e_resize_steps_test.e2e";
  {
    std::ofstream out(path);
    out << "resize_steps 1000 800 4\n"
           "wait_resized 1000 800\n";
  }
  auto parsed = parse_script(path.string());
  CHECK(parsed.size() == 2);
  CHECK(parsed[0].name == "resize_steps" && parsed[0].args.size() == 3);
  CHECK(parsed[1].name == "wait_resized" && parsed[1].args.size() == 2);

  E2ERunner runner;
  runner.load_script(path.string());
  HandleResizeStepsCommand steps;
  HandleWaitResizedCommand wait;
  E2ECommandCleanupSystem cleanup;
  for (int frame = 0; frame < 60 && !runner.is_finished(); frame++) {
    runner.tick(1.f / 60.f);
    f.coll.merge_entity_arrays();
    for (auto &e : f.coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &cmd = e->get<PendingE2ECommand>();
      steps.for_each_with(*e, cmd, 1.f / 60.f);
      wait.for_each_with(*e, cmd, 1.f / 60.f);
      cleanup.for_each_with(*e, cmd, 1.f / 60.f);
    }
    f.coll.cleanup();
    // The frame's render pass: a frame presented at the current size.
    VisibleTextRegistry::instance().clear();
  }
  std::filesystem::remove(path);
  CHECK(runner.is_finished());
  CHECK(!runner.has_failed());
  CHECK(f.resolution() == window_manager::Resolution({1000, 800}));
  CHECK(f.drawable() == window_manager::Resolution({1000, 800}));
}

int main() { return ui_test::run_registered_tests("E2E resize steps"); }
