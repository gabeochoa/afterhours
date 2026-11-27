#ifndef AFTER_HOURS_ENABLE_E2E_TESTING
#define AFTER_HOURS_ENABLE_E2E_TESTING
#endif
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

TEST(simulate_right_press_holds_until_release) {
  test_input::reset_all();
  test_input::simulate_right_press(10, 20);
  auto &m = inj::detail::mouse;
  CHECK(m.pos.x == 10);
  CHECK(m.pos.y == 20);
  CHECK(m.right_down);
  CHECK(m.right_just_pressed);
  CHECK(!m.right_press_read);
  CHECK(m.press_frames == 1);
  CHECK(!m.auto_release);
  CHECK(m.active);
  CHECK(inj::is_mouse_right_button_pressed());
  test_input::reset_frame();
  CHECK(m.right_down);
  CHECK(!m.right_just_released);
  test_input::reset_frame();
  test_input::reset_frame();
  CHECK(m.right_down);
  CHECK(!m.right_just_released);
  test_input::simulate_right_release();
  CHECK(!m.right_down);
  CHECK(m.right_just_released);
  test_input::reset_frame();
  CHECK(!m.right_just_released);
  test_input::reset_all();
}

TEST(right_press_unread_tracking_read_vs_unread) {
  test_input::reset_all();
  test_input::simulate_right_press(5, 6);
  test_input::reset_frame();
  test_input::reset_frame();
  auto unread = inj::take_unread_presses();
  CHECK(unread.size() == 1);
  if (unread.size() == 1) CHECK(unread[0] == "right mouse button");

  test_input::reset_all();
  test_input::simulate_right_press(5, 6);
  CHECK(inj::is_mouse_right_button_pressed());
  test_input::reset_frame();
  test_input::reset_frame();
  CHECK(inj::take_unread_presses().empty());
  test_input::reset_all();
}

struct RightReader : System<> {
  int press_edges = 0;
  int release_edges = 0;
  int held_frames = 0;
  float last_x = 0;
  float last_y = 0;

  bool should_iterate() const override { return false; }

  void once(float) override {
    if (inj::is_mouse_right_button_pressed()) press_edges++;
    if (inj::is_mouse_right_button_down()) {
      held_frames++;
      auto pos = inj::get_mouse_position();
      last_x = pos.x;
      last_y = pos.y;
    }
    if (inj::is_mouse_right_button_released()) release_edges++;
  }
};

struct RunnerFixture {
  EntityCollection collection;
  SystemManager sm;
  E2ERunner runner;
  std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      ("afterhours-right-hold-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));

  RunnerFixture() {
    EntityHelper::set_default_collection(&collection);
    reset_command_error_count();
    test_input::reset_all();
  }

  ~RunnerFixture() {
    test_input::reset_all();
    EntityHelper::set_default_collection(nullptr);
    std::filesystem::remove_all(directory);
  }

  void load(const std::string &script) {
    std::filesystem::create_directory(directory);
    const auto path = directory / "case.e2e";
    std::ofstream(path) << script;
    runner.load_script(path.string());
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

TEST(right_down_script_holds_across_frames) {
  RunnerFixture f;
  register_all_handlers(f.sm);
  auto *reader = new RightReader;
  f.sm.register_update_system(std::unique_ptr<SystemBase>(reader));
  f.load("right_down 10 20\nmouse_move 30 40\nwait_frames 3\nright_up\n");
  f.frames(120);
  CHECK(f.runner.is_finished());
  CHECK(!f.runner.has_failed());
  CHECK(reader->press_edges >= 1);
  CHECK(reader->release_edges >= 1);
  CHECK(reader->held_frames >= 3);
  CHECK(reader->last_x == 30);
  CHECK(reader->last_y == 40);
  CHECK(!inj::detail::mouse.right_down);
}

TEST(right_down_script_unread_press_fails) {
  RunnerFixture f;
  register_all_handlers(f.sm);
  f.load("right_down 10 20\nmouse_move 30 40\nwait_frames 3\nright_up\n");
  f.frames(120);
  CHECK(f.runner.is_finished());
  CHECK(f.runner.has_failed());
}

int main() { return ui_test::run_registered_tests("E2E right hold"); }
