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

TEST(screenshot_fresh_waits_for_a_render_then_captures_current_state) {
  auto &registry = VisibleTextRegistry::instance();
  std::string source = "old";
  std::string texture = "old";
  const auto render = [&] {
    registry.clear();
    texture = source;
  };
  render();
  std::string captured;
  int captures = 0;
  HandleScreenshotCommand handler([&](const std::string &name) {
    captured = texture + ":" + name;
    captures++;
    return true;
  });
  EntityCollection coll;
  Entity &e = coll.createEntity();
  PendingE2ECommand cmd;
  cmd.name = "screenshot_fresh";
  cmd.args = {"menu"};
  cmd.seen_render = static_cast<long long>(registry.generation());
  source = "new";
  handler.for_each_with(e, cmd, 0.f);
  CHECK(cmd.is_retry() && !cmd.is_consumed());
  CHECK(captures == 0);
  render();
  cmd.reset_retry();
  handler.for_each_with(e, cmd, 0.f);
  CHECK(cmd.is_consumed() && cmd.succeeded());
  CHECK(captures == 1);
  CHECK(captured == "new:menu");
  registry.clear();
}

TEST(screenshot_fresh_without_a_callback_fails) {
  EntityCollection coll;
  Entity &e = coll.createEntity();
  PendingE2ECommand cmd;
  cmd.name = "screenshot_fresh";
  cmd.args = {"menu"};
  HandleScreenshotCommand handler(
      std::function<bool(const std::string &name)>{});
  handler.for_each_with(e, cmd, 0.f);
  CHECK(cmd.is_consumed() && !cmd.succeeded());
  CHECK(cmd.error_message ==
        "screenshot_fresh: no screenshot callback registered");
}

TEST(runner_screenshot_fresh_captures_after_the_next_render) {
  auto &registry = VisibleTextRegistry::instance();
  std::string source = "old";
  std::string texture = "old";
  const auto render = [&] {
    registry.clear();
    texture = source;
  };
  render();
  const auto path = std::filesystem::temp_directory_path() /
      ("afterhours-fresh-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".e2e");
  std::ofstream(path) << "screenshot_fresh freshy\n";
  ui_test::ImmTestHarness h;
  E2ERunner runner;
  std::string captured;
  int captures = 0;
  runner.set_screenshot_callback([&](const std::string &name) {
    captured = texture + ":" + name;
    captures++;
    return true;
  });
  runner.load_script(path.string());
  runner.tick(0.1f);
  CHECK(runner.needs_render());
  CHECK(!runner.is_idle_waiting());
  CHECK(captures == 0);
  source = "new";
  render();
  for (int frame = 0; frame < 20 && !runner.is_finished(); ++frame)
    runner.tick(0.1f);
  CHECK(captures == 1);
  CHECK(captured == "new:freshy");
  CHECK(!runner.needs_render());
  CHECK(!runner.has_failed());
  std::filesystem::remove(path);
  registry.clear();
}

int main() { return ui_test::run_registered_tests("E2E screenshot"); }
