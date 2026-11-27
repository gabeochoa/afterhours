#ifndef AFTER_HOURS_ENABLE_E2E_TESTING
#define AFTER_HOURS_ENABLE_E2E_TESTING
#endif
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::testing;

struct RunnerFixture {
  EntityCollection collection;
  SystemManager sm;
  E2ERunner runner;
  std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      ("afterhours-script-info-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));

  RunnerFixture() {
    EntityHelper::set_default_collection(&collection);
    reset_command_error_count();
    test_input::reset_all();
    std::filesystem::create_directory(directory);
  }

  ~RunnerFixture() {
    test_input::reset_all();
    EntityHelper::set_default_collection(nullptr);
    std::filesystem::remove_all(directory);
  }

  std::filesystem::path write_script(const std::string &name,
                                     const std::string &text) {
    const auto path = directory / name;
    std::ofstream(path) << text;
    return path;
  }

  void frames(int n) {
    const float dt = 1.f / 60.f;
    for (int i = 0; i < n && !runner.is_finished(); ++i) {
      runner.tick(dt);
      sm.run(dt);
      test_input::reset_frame();
    }
  }
};

TEST(current_script_none_then_single) {
  RunnerFixture f;
  CHECK(!f.runner.current_script().has_value());
  const auto path = f.write_script("solo.e2e", "wait_frames 1\n");
  f.runner.load_script(path.string());
  auto info = f.runner.current_script();
  CHECK(info.has_value());
  if (info) {
    CHECK(info->index == 0);
    CHECK(info->path == path.string());
    CHECK(info->name == "solo");
  }
}

TEST(batch_current_script_and_reset_callbacks) {
  RunnerFixture f;
  register_all_handlers(f.sm);
  const auto first = f.write_script("a_first.e2e", "# spawn: alpha\nwait_frames 1\n");
  const auto second = f.write_script("b_second.e2e", "# spawn: beta\nwait_frames 1\n");
  const auto third = f.write_script("c_third.e2e", "# spawn: gamma\nwait_frames 1\n");
  f.write_script("ignored.txt", "wait_frames 1\n");

  int zero_calls = 0;
  std::vector<E2ERunner::ScriptInfo> next_seen;
  std::vector<std::string> next_headers;
  f.runner.set_reset_callback(std::function<void()>{[&] { zero_calls++; }});
  f.runner.set_reset_callback(
      std::function<void(const E2ERunner::ScriptInfo &)>{
          [&](const E2ERunner::ScriptInfo &next) {
            next_seen.push_back(next);
            std::ifstream in(next.path);
            std::string header;
            std::getline(in, header);
            next_headers.push_back(header);
          }});

  f.runner.load_scripts_from_directory(f.directory.string());
  auto info = f.runner.current_script();
  CHECK(info.has_value());
  if (info) {
    CHECK(info->index == 0);
    CHECK(info->path == first.string());
    CHECK(info->name == "a_first");
  }

  std::vector<size_t> indices_seen;
  const float dt = 1.f / 60.f;
  for (int i = 0; i < 240 && !f.runner.is_finished(); ++i) {
    if (auto current = f.runner.current_script()) {
      if (indices_seen.empty() || indices_seen.back() != current->index)
        indices_seen.push_back(current->index);
    }
    f.runner.tick(dt);
    f.sm.run(dt);
    test_input::reset_frame();
  }

  CHECK(f.runner.is_finished());
  CHECK(!f.runner.has_failed());
  CHECK(indices_seen == std::vector<size_t>({0, 1, 2}));
  CHECK(zero_calls == 3);
  CHECK(next_seen.size() == 2);
  if (next_seen.size() == 2) {
    CHECK(next_seen[0].index == 1);
    CHECK(next_seen[0].path == second.string());
    CHECK(next_seen[0].name == "b_second");
    CHECK(next_seen[1].index == 2);
    CHECK(next_seen[1].path == third.string());
    CHECK(next_seen[1].name == "c_third");
  }
  CHECK(next_headers ==
        std::vector<std::string>({"# spawn: beta", "# spawn: gamma"}));
  CHECK(!f.runner.current_script().has_value());
}

int main() { return ui_test::run_registered_tests("E2E script info"); }
