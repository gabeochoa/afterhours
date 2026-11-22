#include <algorithm>
#include <cstdio>
#include <format>

#include "../../../../ah.h"
#include "../../../../src/plugins/e2e_testing/e2e_testing.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
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

int main() {
  const auto stick = static_cast<input::GamepadAxis>(gamepad_axes::LEFT_X);
  input::add_singleton_components(
      EntityHelper::createEntity(),
      {
          {Left, {input::GamepadAxisWithDir{stick, -1}}},
          {Right, {input::GamepadAxisWithDir{stick, 1}}},
          {Jump, {static_cast<input::GamepadButton>(
                     gamepad_buttons::RIGHT_FACE_DOWN)}},
      });

  E2ERunner runner;
  runner.load_script(SCRIPT_PATH);

  SystemManager systems;
  systems.register_update_system(std::make_unique<input::InputSystem>());
  systems.register_update_system(std::make_unique<Walker>());
  register_all_handlers(systems);

  const float dt = 1.f / 60.f;
  while (!runner.is_finished()) {
    test_input::reset_frame();
    runner.tick(dt);
    systems.run(dt);
  }
  runner.print_results();
  return runner.has_failed() ? 1 : 0;
}
