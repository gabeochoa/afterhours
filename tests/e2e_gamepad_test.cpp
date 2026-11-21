#include "ui_test_harness.h"
#include <afterhours/src/plugins/e2e_testing/e2e_testing.h>
#include <afterhours/src/plugins/input_system.h>

#include <cmath>
#include <string>
#include <vector>

using namespace afterhours;
using namespace afterhours::testing;
namespace inj = afterhours::testing::input_injector;

enum Action { Jump = 1, Right = 2 };

struct GamepadFixture {
  EntityCollection collection;
  Entity &entity = collection.createEntity();
  input::InputCollector collector;
  input::ProvidesMaxGamepadID gamepads;
  input::ProvidesInputMapping mapper{{
      {Jump, {static_cast<input::GamepadButton>(
                 gamepad_buttons::RIGHT_FACE_DOWN)}},
      {Right, {input::GamepadAxisWithDir{
                  static_cast<input::GamepadAxis>(gamepad_axes::LEFT_X), 1}}},
  }};
  input::InputSystem system;

  GamepadFixture() {
    EntityHelper::set_default_collection(&collection);
    collection.merge_entity_arrays();
    test_input::reset_all();
  }
  ~GamepadFixture() {
    test_input::reset_all();
    EntityHelper::set_default_collection(nullptr);
  }
  void frame() {
    inj::reset_frame();
    system.for_each_with(entity, collector, gamepads, mapper, 1.f / 60.f);
  }
  std::string run(const std::string &name, std::vector<std::string> args) {
    PendingE2ECommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    HandleGamepadConnectCommand{}.for_each_with(entity, cmd, 0.f);
    HandleGamepadButtonCommand{}.for_each_with(entity, cmd, 0.f);
    HandleGamepadAxisCommand{}.for_each_with(entity, cmd, 0.f);
    CHECK(cmd.is_consumed());
    return cmd.error_message;
  }
  static const input::ActionDone *find(const std::vector<input::ActionDone> &v,
                                       int action, int pad = 0) {
    for (const auto &a : v)
      if (a.action == action && a.id == pad) return &a;
    return nullptr;
  }
};

TEST(connect_makes_virtual_pads_visible) {
  GamepadFixture f;
  CHECK(!input::is_gamepad_available(0));
  CHECK(f.run("gamepad_connect", {"0"}).empty());
  CHECK(f.run("gamepad_connect", {"1"}).empty());
  f.frame();
  CHECK(input::is_gamepad_available(1));
  CHECK(f.gamepads.max_gamepad_available == 1);
  CHECK(f.run("gamepad_disconnect", {"1"}).empty());
  f.frame();
  CHECK(!input::is_gamepad_available(1));
  CHECK(f.gamepads.max_gamepad_available == 0);
}

TEST(button_tap_presses_once_and_releases) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  CHECK(f.run("gamepad_button", {"0", "right_face_down"}).empty());
  f.frame();
  const auto *pressed = GamepadFixture::find(f.collector.inputs_pressed, Jump);
  CHECK(pressed != nullptr);
  CHECK(pressed && pressed->medium == input::DeviceMedium::GamepadButton);
  CHECK(GamepadFixture::find(f.collector.inputs, Jump) != nullptr);
  CHECK(f.collector.device_activity.preferred() == input::Device::Gamepad);
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump) == nullptr);
  CHECK(GamepadFixture::find(f.collector.inputs, Jump) == nullptr);
}

TEST(button_down_holds_until_up) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  CHECK(f.run("gamepad_down", {"0", "RIGHT_FACE_DOWN"}).empty());
  for (int i = 0; i < 4; ++i) {
    f.frame();
    CHECK(GamepadFixture::find(f.collector.inputs, Jump) != nullptr);
    CHECK((GamepadFixture::find(f.collector.inputs_pressed, Jump) != nullptr) ==
          (i == 0));
  }
  CHECK(f.run("gamepad_up", {"0", "RIGHT_FACE_DOWN"}).empty());
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs, Jump) == nullptr);
}

TEST(axis_holds_until_changed) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  CHECK(f.run("gamepad_axis", {"0", "LEFT_X", "0.8"}).empty());
  for (int i = 0; i < 3; ++i) {
    f.frame();
    const auto *right = GamepadFixture::find(f.collector.inputs, Right);
    CHECK(right != nullptr);
    CHECK(right && std::fabs(right->amount_pressed - 0.8f) < 1e-5f);
  }
  f.run("gamepad_axis", {"0", "LEFT_X", "-0.8"});
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs, Right) == nullptr);
  f.run("gamepad_axis", {"0", "LEFT_X", "0.1"});
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs, Right) == nullptr);
}

TEST(second_pad_reports_its_own_id) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  f.run("gamepad_connect", {"1"});
  f.run("gamepad_button", {"1", "RIGHT_FACE_DOWN"});
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump, 1) != nullptr);
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump, 0) == nullptr);
}

TEST(bad_arguments_fail_with_a_reason) {
  GamepadFixture f;
  CHECK(f.run("gamepad_button", {"0", "RIGHT_FACE_DOWN"})
            .find("not connected") != std::string::npos);
  CHECK(f.run("gamepad_connect", {"8"}).find("0-7") != std::string::npos);
  CHECK(f.run("gamepad_connect", {}).find("0-7") != std::string::npos);
  f.run("gamepad_connect", {"0"});
  CHECK(f.run("gamepad_down", {"0", "A"}).find("unknown gamepad button") !=
        std::string::npos);
  CHECK(f.run("gamepad_axis", {"0", "SIDEWAYS", "0.5"})
            .find("unknown gamepad axis") != std::string::npos);
  CHECK(f.run("gamepad_axis", {"0", "LEFT_X", "2"}).find("-1 to 1") !=
        std::string::npos);
  CHECK(f.run("gamepad_axis", {"0", "LEFT_X"}).find("-1 to 1") !=
        std::string::npos);
}

TEST(double_press_in_one_frame_registers_twice) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  inj::set_gamepad_button_down(0, gamepad_buttons::RIGHT_FACE_DOWN);
  inj::set_gamepad_button_down(0, gamepad_buttons::RIGHT_FACE_DOWN);
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump) != nullptr);
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump) != nullptr);
  f.frame();
  CHECK(GamepadFixture::find(f.collector.inputs_pressed, Jump) == nullptr);
}

TEST(press_count_saturates_at_255) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  for (int i = 0; i < 300; ++i)
    inj::set_gamepad_button_down(0, gamepad_buttons::RIGHT_FACE_DOWN);
  const auto count =
      inj::detail::gamepads[0].press_count[gamepad_buttons::RIGHT_FACE_DOWN];
  CHECK(count == 255);
}

TEST(reset_disconnects_every_pad) {
  GamepadFixture f;
  f.run("gamepad_connect", {"0"});
  f.run("gamepad_down", {"0", "RIGHT_FACE_DOWN"});
  f.run("gamepad_axis", {"0", "LEFT_X", "1"});
  test_input::reset_all();
  f.frame();
  CHECK(!input::is_gamepad_available(0));
  CHECK(f.collector.inputs.empty());
}

int main() { return ui_test::run_registered_tests("E2E gamepad injection"); }
