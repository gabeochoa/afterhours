// e2e_barrier_test.cpp
// await_frame holds a script until a frame whose layout and render both ran
// after it, and block_until() gives any command that barrier plus a
// wall-clock deadline. Before this, retry() kept a command pending but the
// runner dispatched the next line anyway, and the 30-tick budget expired in
// a headless host long before a render.
#include "ui_test_harness.h"

#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace afterhours;
using namespace afterhours::testing;

namespace {

// A host loop without a window: runner, then handlers, then cleanup, and a
// UI render only when asked for, the way a batched headless host behaves.
struct Host {
  EntityCollection coll;
  HandleAwaitFrameCommand await_frame;
  HandleClickCommand click;
  HandleNativeResizeCommand native_resize;
  E2ECommandCleanupSystem cleanup;
  E2ERunner runner;
  std::filesystem::path script;
  std::vector<std::string> dispatched;

  explicit Host(const std::string &text) {
    EntityHelper::set_default_collection(&coll);
    test_input::reset_all();
    reset_command_error_count();
    script = std::filesystem::temp_directory_path() /
             ("afh_barrier_" +
              std::to_string(std::chrono::steady_clock::now()
                                 .time_since_epoch()
                                 .count()) +
              ".e2e");
    std::ofstream(script) << text;
    runner.load_script(script.string());
  }
  ~Host() {
    std::filesystem::remove(script);
    test_input::reset_all();
    EntityHelper::set_default_collection(nullptr);
  }

  void tick() {
    runner.tick(1.f / 60.f);
    coll.merge_entity_arrays();
    for (const auto &e : coll.get_entities()) {
      if (!e || !e->has<PendingE2ECommand>()) continue;
      auto &cmd = e->get<PendingE2ECommand>();
      // Keyed by script line: entity ids are recycled.
      if (std::find(seen.begin(), seen.end(), cmd.line_number) ==
          seen.end()) {
        seen.push_back(cmd.line_number);
        dispatched.push_back(cmd.name);
      }
      await_frame.for_each_with(*e, cmd, 0.f);
      click.for_each_with(*e, cmd, 0.f);
      native_resize.for_each_with(*e, cmd, 0.f);
      cleanup.for_each_with(*e, cmd, 0.f);
    }
    coll.cleanup();
  }
  void render() { VisibleTextRegistry::instance().clear(); }
  bool was_dispatched(const std::string &name) const {
    return std::find(dispatched.begin(), dispatched.end(), name) !=
           dispatched.end();
  }

 private:
  std::vector<int> seen;
};

std::string captured_log;

} // namespace

TEST(await_frame_waits_for_two_renders_and_holds_the_script) {
  Host h("await_frame 5\nclick 10 10\n");
  // Many more logic ticks than the 30-frame budget, and no render: the
  // barrier must neither time out nor let the click through.
  for (int i = 0; i < 120; ++i) h.tick();
  CHECK(h.was_dispatched("await_frame"));
  CHECK(!h.was_dispatched("click"));
  CHECK(get_command_error_count() == 0);

  // One render could close a frame whose layout predates the barrier.
  h.render();
  for (int i = 0; i < 10; ++i) h.tick();
  CHECK(!h.was_dispatched("click"));

  h.render();
  for (int i = 0; i < 10 && !h.runner.is_finished(); ++i) h.tick();
  CHECK(h.was_dispatched("click"));
  CHECK(h.runner.is_finished());
  CHECK(!h.runner.has_failed());
}

TEST(await_frame_fails_with_its_reason_at_the_deadline) {
  captured_log.clear();
  log_sink_fn = [](const char *, const char *message) {
    captured_log += message;
    captured_log += "\n";
  };
  Host h("await_frame 0.05\nclick 10 10\n");
  for (int i = 0; i < 5; ++i) h.tick();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  for (int i = 0; i < 20 && !h.runner.is_finished(); ++i) h.tick();
  log_sink_fn = nullptr;
  CHECK(h.runner.is_finished());
  CHECK(h.runner.has_failed());
  CHECK(captured_log.find("await_frame timed out") != std::string::npos);
  CHECK(captured_log.find("0 of 2 UI render passes") != std::string::npos);
}

TEST(block_until_falls_back_to_a_wall_clock_deadline) {
  EntityCollection coll;
  EntityHelper::set_default_collection(&coll);
  reset_command_error_count();
  Entity &e = coll.createEntity();
  auto &cmd = e.addComponent<PendingE2ECommand>();
  cmd.name = "wait_for_worker";
  coll.merge_entity_arrays();
  E2ECommandCleanupSystem cleanup;

  // 100 ticks would have timed a plain retry() out at 30.
  for (int i = 0; i < 100; ++i) {
    cmd.block_until(0.05f);
    cleanup.for_each_with(e, cmd, 0.f);
  }
  CHECK(cmd.blocking);
  CHECK(!cmd.is_consumed());
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  cmd.block_until(0.05f);
  cleanup.for_each_with(e, cmd, 0.f);
  CHECK(cmd.is_consumed());
  CHECK(!cmd.succeeded());
  CHECK(cmd.error_message.find("wall-clock deadline") != std::string::npos);
  CHECK(get_command_error_count() == 1);
  reset_command_error_count();
  EntityHelper::set_default_collection(nullptr);
}

TEST(retry_alone_still_does_not_hold_the_script) {
  // Unchanged contract: an expect_text that is still retrying does not stop
  // the next line, only a blocking command does.
  Host h("expect_text never_rendered\nclick 10 10\n");
  for (int i = 0; i < 10; ++i) h.tick();
  CHECK(h.was_dispatched("expect_text"));
  CHECK(h.was_dispatched("click"));
}

TEST(native_resize_is_explicitly_unsupported_without_a_window) {
  Host h("native_resize 640 480\n");
  PendingE2ECommand cmd;
  cmd.name = "native_resize";
  cmd.args = {"640", "480"};
  Entity &e = h.coll.createEntity();
  h.native_resize.for_each_with(e, cmd, 0.f);
  CHECK(cmd.is_consumed());
  CHECK(!cmd.succeeded());
  CHECK(cmd.error_message.find("unsupported") != std::string::npos);

  PendingE2ECommand bad;
  bad.name = "native_resize";
  bad.args = {"0", "480"};
  h.native_resize.for_each_with(e, bad, 0.f);
  CHECK(!bad.succeeded());
  CHECK(bad.error_message.find("invalid dimensions") != std::string::npos);
}

int main() { return ui_test::run_registered_tests("E2E barriers"); }
