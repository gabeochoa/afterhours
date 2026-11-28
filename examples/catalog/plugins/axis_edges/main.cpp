#include <cstdio>

#include "../../../example_helper.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
using namespace afterhours::examples;

namespace {
const float trigger_pull[] = {0.f, .1f, .9f, 1.f, 1.f, .6f, .1f, 0.f, .8f, 1.f};
int frame = 0;
}

namespace raylib {
bool IsGamepadAvailable(int pad) { return pad == 0; }
float GetGamepadAxisMovement(int, int axis) {
  return axis == GAMEPAD_AXIS_RIGHT_TRIGGER ? trigger_pull[frame] : 0.f;
}
}

enum Action { Fire };

int main() {
  const auto trigger = raylib::GAMEPAD_AXIS_RIGHT_TRIGGER;
  input::add_singleton_components(
      EntityHelper::createEntity(),
      {{Fire, {input::GamepadAxisWithDir{trigger, 1}}}});

  Checks checks;
  auto systems = update_systems<input::InputSystem>();

  int held_frames = 0;
  int shots = 0;
  for (frame = 0; frame < static_cast<int>(std::size(trigger_pull)); ++frame) {
    systems.run(1.f / 60.f);
    auto collector = input::get_input_collector();
    const bool held = !collector.inputs().empty();
    const bool fired = !collector.inputs_pressed().empty();
    held_frames += held;
    shots += fired;
    std::printf("frame %d trigger %.1f held %d fired %d edge %d\n", frame,
                static_cast<double>(trigger_pull[frame]), held, fired,
                input::is_gamepad_axis_pressed(0, trigger, 1));
  }
  std::printf("held for %d frames, fired %d shots\n", held_frames, shots);
  checks.check(held_frames == 6, "trigger held for 6 frames");
  checks.check(shots == 2, "trigger fired twice");
  return checks.finish("axis_edges");
}
