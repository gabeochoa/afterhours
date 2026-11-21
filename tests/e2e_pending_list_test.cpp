#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/harness.h>

#include <chrono>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

struct ScriptDir {
  std::filesystem::path directory = std::filesystem::temp_directory_path() /
      ("afterhours-pending-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  ScriptDir() { std::filesystem::create_directory(directory); }
  ~ScriptDir() { std::filesystem::remove_all(directory); }
  std::string add(const std::string &name, const std::string &text) {
    auto path = directory / (name + ".e2e");
    std::ofstream(path) << text;
    return path.string();
  }
};

// Drive the runner like the results test does: tick, then consume commands
// and run the cleanup system over them.
static void drive(E2ERunner &runner, auto &&on_command, int max_frames = 200) {
  auto &coll = EntityHelper::get_default_collection();
  E2ECommandCleanupSystem cleanup;
  for (int frame = 0; frame < max_frames && !runner.is_finished(); ++frame) {
    runner.tick(0.1f);
    coll.merge_entity_arrays();
    for (auto &e : coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &command = e->get<PendingE2ECommand>();
      on_command(command, frame);
      cleanup.for_each_with(*e, command, 0.f);
    }
    coll.cleanup();
  }
}

TEST(pending_list_stays_serial_with_thousands_of_decoys) {
  ui_test::ImmTestHarness h;
  auto &coll = EntityHelper::get_default_collection();
  for (int i = 0; i < 2000; ++i) coll.createEntity();
  coll.merge_entity_arrays();
  ScriptDir dir;
  E2ERunner runner;
  runner.load_script(dir.add("many", "ok\nok\nok\n"));
  size_t peak = 0;
  drive(runner, [&](PendingE2ECommand &cmd, int) {
    peak = std::max(peak, runner.pending_command_count());
    if (cmd.is("ok")) cmd.consume();
  });
  CHECK(runner.is_finished());
  CHECK(!runner.has_failed());
  // Serial dispatch keeps at most one pending command; the runner's check no
  // longer scans the whole entity set to find it.
  CHECK(peak <= 1);
  CHECK(runner.pending_command_count() == 0);
}

TEST(a_late_command_still_blocks_until_consumed) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  E2ERunner runner;
  runner.load_script(dir.add("slow", "ok\n"));
  int consumed_at = -1;
  drive(runner, [&](PendingE2ECommand &cmd, int frame) {
    if (cmd.is("ok") && frame >= 6 && consumed_at < 0) {
      cmd.consume();
      consumed_at = frame;
    }
  });
  CHECK(consumed_at >= 6);
  CHECK(runner.is_finished());
  CHECK(!runner.has_failed());
}

TEST(an_unconsumed_command_still_fails_instead_of_passing) {
  ui_test::ImmTestHarness h;
  ScriptDir dir;
  E2ERunner runner;
  runner.load_script(dir.add("ghost", "ghost\n"));
  drive(runner, [](PendingE2ECommand &, int) {}, 300);
  CHECK(runner.is_finished());
  CHECK(runner.has_failed());
  CHECK(runner.pending_command_count() == 0);
}

int main() { return ui_test::run_registered_tests("E2E pending list"); }
