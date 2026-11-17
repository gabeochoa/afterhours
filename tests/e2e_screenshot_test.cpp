#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

static PendingE2ECommand screenshot_with(HandleScreenshotCommand handler) {
  EntityCollection coll;
  Entity &e = coll.createEntity();
  PendingE2ECommand cmd;
  cmd.name = "screenshot";
  cmd.args = {"menu"};
  handler.for_each_with(e, cmd, 0.f);
  return cmd;
}

TEST(a_failed_capture_fails_the_command) {
  auto cmd = screenshot_with(
      HandleScreenshotCommand([](const std::string &) { return false; }));
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message == "screenshot 'menu' failed to capture");
}

TEST(a_successful_or_void_capture_passes) {
  auto ok = screenshot_with(
      HandleScreenshotCommand([](const std::string &) { return true; }));
  CHECK(ok.is_consumed() && ok.succeeded());
  std::string seen;
  auto legacy = screenshot_with(HandleScreenshotCommand(
      [&](const std::string &name) { seen = name; }));
  CHECK(legacy.is_consumed() && legacy.succeeded());
  CHECK(seen == "menu");
  auto empty = screenshot_with(
      HandleScreenshotCommand(std::function<void(const std::string &)>{}));
  CHECK(empty.is_consumed() && empty.succeeded());
}

static bool run_script_with(auto callback) {
  const auto path = std::filesystem::temp_directory_path() /
      ("afterhours-shot-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".e2e");
  std::ofstream(path) << "screenshot title\n";
  ui_test::ImmTestHarness h;
  E2ERunner runner;
  runner.set_screenshot_callback(callback);
  runner.load_script(path.string());
  auto &coll = EntityHelper::get_default_collection();
  E2ECommandCleanupSystem cleanup;
  for (int frame = 0; frame < 20 && !runner.is_finished(); ++frame) {
    runner.tick(0.1f);
    coll.merge_entity_arrays();
    for (auto &e : coll.get_entities())
      if (e && e->has<PendingE2ECommand>())
        cleanup.for_each_with(*e, e->get<PendingE2ECommand>(), 0.f);
    coll.cleanup();
  }
  std::filesystem::remove(path);
  return runner.has_failed();
}

TEST(the_runner_callback_fails_the_script) {
  CHECK(run_script_with([](const std::string &) { return false; }));
  CHECK(!run_script_with([](const std::string &) { return true; }));
  CHECK(!run_script_with([](const std::string &) {}));
}

int main() { return ui_test::run_registered_tests("E2E screenshot"); }
