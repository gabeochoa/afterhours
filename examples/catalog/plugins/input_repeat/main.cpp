#include <cstdio>

#include "../../../../src/plugins/input_system.h"

using afterhours::input;

int main() {
  input::Repeat move_left{.delay = 0.17f, .rate = 0.05f};
  const float dt = 1.f / 60.f;
  int column = 20;

  for (int frame = 0; frame < 60; ++frame) {
    const bool held = frame < 30;
    const int steps = move_left.tick(held, dt);
    column -= steps;
    if (steps > 0)
      std::printf("frame %2d: moved %d, column %d\n", frame, steps, column);
  }

  std::printf("held half a second: moved %d columns\n", 20 - column);
  return column == 12 ? 0 : 1;
}
