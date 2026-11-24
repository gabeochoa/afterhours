#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

struct KeyReader : System<> {
  int skip_calls = 0;

  bool should_iterate() const override { return false; }
  void once(float) override {
    if (skip_calls > 0) {
      skip_calls--;
      return;
    }
    inj::consume_press(keys::F);
  }
};

struct MouseReader : System<> {
  bool should_iterate() const override { return false; }
  void once(float) override { inj::is_mouse_button_pressed(); }
};

struct MouseDownReader : System<> {
  bool should_iterate() const override { return false; }
  void once(float) override {
    test_input::is_mouse_button_down(0, [](int) { return false; });
  }
};

struct RunnerFixture {
  EntityCollection collection;
  SystemManager sm;
  E2ERunner runner;
  std::filesystem::path directory =
      std::filesystem::temp_directory_path() /
      ("afterhours-unread-press-" + std::to_string(
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

TEST(unread_key_press_fails_and_names_the_key) {
  static std::string captured;
  captured.clear();
  const auto previous = log_sink_fn;
  log_sink_fn = [](const char *, const char *message) { captured += message; };

  RunnerFixture f;
  register_all_handlers(f.sm);
  f.load("key F\n");
  f.frames(60);

  log_sink_fn = previous;
  CHECK(f.runner.is_finished());
  CHECK(f.runner.has_failed());
  CHECK(get_command_error_count() == 0);
  CHECK(captured.find("key 'F'") != std::string::npos);
  CHECK(captured.find("never read") != std::string::npos);
}

TEST(unread_mouse_press_fails) {
  RunnerFixture f;
  register_all_handlers(f.sm);
  f.load("click 10 10\n");
  f.frames(60);
  CHECK(f.runner.is_finished());
  CHECK(f.runner.has_failed());
  CHECK(get_command_error_count() == 0);
}

TEST(read_presses_still_pass) {
  {
    RunnerFixture f;
    register_all_handlers(f.sm);
    f.sm.register_update_system(std::make_unique<KeyReader>());
    f.load("key F\n");
    f.frames(60);
    CHECK(f.runner.is_finished());
    CHECK(!f.runner.has_failed());
  }
  {
    RunnerFixture f;
    register_all_handlers(f.sm);
    f.sm.register_update_system(std::make_unique<MouseReader>());
    f.load("click 10 10\n");
    f.frames(60);
    CHECK(f.runner.is_finished());
    CHECK(!f.runner.has_failed());
  }
}

TEST(read_mouse_down_press_still_passes) {
  RunnerFixture f;
  test_input::detail::test_mode = true;
  register_all_handlers(f.sm);
  f.sm.register_update_system(std::make_unique<MouseDownReader>());
  f.load("click 10 10\n");
  f.frames(60);
  test_input::detail::test_mode = false;
  CHECK(f.runner.is_finished());
  CHECK(!f.runner.has_failed());
}

TEST(press_read_late_within_its_window_passes) {
  RunnerFixture f;
  register_all_handlers(f.sm);
  auto *reader = new KeyReader;
  reader->skip_calls = 1;
  f.sm.register_update_system(std::unique_ptr<SystemBase>(reader));
  f.load("key F\n");
  f.frames(60);
  CHECK(f.runner.is_finished());
  CHECK(!f.runner.has_failed());
}

TEST(held_key_observed_held_is_not_unread) {
  struct HeldReader : System<> {
    bool should_iterate() const override { return false; }
    void once(float) override { inj::is_key_down(keys::W); }
  };
  RunnerFixture f;
  register_all_handlers(f.sm);
  f.sm.register_update_system(std::make_unique<HeldReader>());
  f.load("hold_key W\nrelease_key W\n");
  f.frames(60);
  CHECK(f.runner.is_finished());
  CHECK(!f.runner.has_failed());
}

int main() { return ui_test::run_registered_tests("E2E unread press"); }
