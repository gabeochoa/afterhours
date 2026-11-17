#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;

TEST(loading_a_script_marks_the_process_as_under_e2e) {
  CHECK(!is_e2e_running());
  const auto path = std::filesystem::temp_directory_path() /
      ("afterhours-running-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()) + ".e2e");
  std::ofstream(path) << "wait_frames 1\n";
  E2ERunner runner;
  runner.load_script(path.string());
  CHECK(is_e2e_running());
  std::filesystem::remove(path);
}

int main() { return ui_test::run_registered_tests("E2E is_e2e_running"); }
