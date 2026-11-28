#define AFTER_HOURS_EXAMPLE_E2E
#include "../../../example_helper.h"

#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
using namespace afterhours::examples;
using namespace afterhours::testing;

enum Action { Left, Right, Jump };

struct Walker : System<> {
  int x = 0;
  int jumps = 0;

  bool should_iterate() const override { return false; }
  void once(float) override {
    auto collector = input::get_input_collector();
    for (const auto &done : collector.inputs()) {
      if (done.action == Right) x++;
      if (done.action == Left) x--;
    }
    for (const auto &done : collector.inputs_pressed())
      if (done.action == Jump) jumps++;
    x = std::clamp(x, 0, 10);
    auto &text = VisibleTextRegistry::instance();
    text.clear();
    text.register_text(std::format("x: {}", x));
    text.register_text(std::format("jumps: {}", jumps));
  }
};

int main(int argc, char *argv[]) {
  E2EHarness harness(argc, argv);
  if (!harness.has_script()) harness.args.script_path = SCRIPT_PATH;

  const auto stick = static_cast<input::GamepadAxis>(gamepad_axes::LEFT_X);
  input::add_singleton_components(
      EntityHelper::createEntity(),
      {
          {Left, {input::GamepadAxisWithDir{stick, -1}}},
          {Right, {input::GamepadAxisWithDir{stick, 1}}},
          {Jump, {static_cast<input::GamepadButton>(
                     gamepad_buttons::RIGHT_FACE_DOWN)}},
      });

  harness.add_systems(std::make_unique<input::InputSystem>(),
                      std::make_unique<Walker>());
  return harness.run([&] { test_input::reset_frame(); });
}
