#define AFTER_HOURS_ENABLE_E2E_TESTING
#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/runner.h>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

struct KeyFixture {
  EntityCollection collection;
  Entity &entity = collection.createEntity();
  HandleKeyReleaseSystem release;

  KeyFixture() {
    EntityHelper::set_default_collection(&collection);
    collection.merge_entity_arrays();
    test_input::reset_all();
    key_release_detail::reset();
  }
  ~KeyFixture() {
    test_input::reset_all();
    key_release_detail::reset();
    EntityHelper::set_default_collection(nullptr);
  }
  void key(const std::string &chord) {
    PendingE2ECommand command;
    command.name = "key";
    command.args = {chord};
    HandleKeyCommand{}.for_each_with(entity, command, 0.f);
    CHECK(command.is_consumed());
    CHECK(command.error_message.empty());
  }
  void frame() {
    inj::reset_frame();
    release.once(1.f / 60.f);
  }
  void released() {
    for (int key : {keys::LEFT_CONTROL, keys::LEFT_SHIFT, keys::LEFT_ALT,
                    keys::LEFT_SUPER, keys::A}) {
      CHECK(!inj::is_key_down(key));
      CHECK(!inj::consume_press(key));
    }
    CHECK(key_release_detail::release_countdown == 0);
  }
};

TEST(super_aliases_reach_the_action_and_release) {
  for (const char *alias : {"CMD", "cmd", "Cmd", "SUPER", "super", "Super",
                            "WIN", "win", "Win", "META", "meta", "Meta"}) {
    KeyFixture f;
    const std::string chord = std::string(alias) + "+A";
    const auto parsed = parse_key_combo(chord);
    CHECK(parsed.super);
    CHECK(!parsed.ctrl);
    CHECK(parsed.key == keys::A);
    f.key(chord);
    CHECK(!inj::consume_press(keys::A));
    f.frame();
    CHECK(inj::consume_press(keys::A));
    CHECK(inj::is_key_down(keys::LEFT_SUPER));
    CHECK(!inj::is_key_down(keys::LEFT_CONTROL));
    CHECK(!inj::consume_press(keys::LEFT_SUPER));
    f.frame();
    f.released();
    f.key("A");
    f.frame();
    CHECK(inj::consume_press(keys::A));
    CHECK(!inj::is_key_down(keys::LEFT_SUPER));
    CHECK(!inj::is_key_down(keys::LEFT_CONTROL));
    f.frame();
    f.released();
  }
}

TEST(control_and_combined_modifiers) {
  for (const std::string chord : {"CTRL+A", "Ctrl+Shift+Option+Cmd+A"}) {
    KeyFixture f;
    const bool combined = chord != "CTRL+A";
    f.key(chord);
    f.frame();
    CHECK(inj::consume_press(keys::A));
    CHECK(inj::is_key_down(keys::LEFT_CONTROL));
    CHECK(inj::is_key_down(keys::LEFT_SHIFT) == combined);
    CHECK(inj::is_key_down(keys::LEFT_ALT) == combined);
    CHECK(inj::is_key_down(keys::LEFT_SUPER) == combined);
    for (int modifier : {keys::LEFT_CONTROL, keys::LEFT_SHIFT, keys::LEFT_ALT,
                         keys::LEFT_SUPER})
      CHECK(!inj::consume_press(modifier));
    f.frame();
    f.released();
  }
}

TEST(reset_and_skip_clear_pending_chords) {
  for (int action = 0; action < 3; ++action) {
    KeyFixture f;
    f.key("CTRL+SHIFT+ALT+SUPER+A");
    E2ERunner runner;
    if (action == 0) {
      PendingE2ECommand command;
      command.name = "reset_test_state";
      HandleResetTestStateCommand{}.for_each_with(f.entity, command, 0.f);
      CHECK(command.is_consumed());
    }
    if (action == 1) runner.reset();
    if (action == 2) runner.skip_current_script();
    f.released();
    f.frame();
    f.released();
  }
}

TEST(timeout_clears_pending_chords) {
  const auto directory = std::filesystem::temp_directory_path() /
      ("afterhours-key-test-" + std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count()));
  const bool created = std::filesystem::create_directory(directory);
  CHECK(created);
  if (!created) return;
  struct Cleanup {
    std::filesystem::path path;
    ~Cleanup() { std::filesystem::remove_all(path); }
  } cleanup{directory};
  const auto script = cleanup.path / "shortcut.e2e";
  std::ofstream(script) << "key SUPER+A\n";
  KeyFixture f;
  E2ERunner runner;
  runner.load_script(script.string());
  f.key("CTRL+SHIFT+ALT+SUPER+A");
  auto &pending = f.entity.addComponent<PendingE2ECommand>();
  pending.name = "key";
  pending.args = {"SUPER+A"};
  runner.set_timeout(0.001f);
  runner.tick(1.f);
  CHECK(runner.has_timed_out());
  CHECK(runner.is_finished());
  CHECK(pending.is_consumed());
  f.released();
  f.frame();
  f.released();
}

int main() { return ui_test::run_registered_tests("E2E key chords"); }
