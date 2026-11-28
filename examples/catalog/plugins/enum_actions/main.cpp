#include <cstdio>

#include "../../../example_helper.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
using namespace afterhours::examples;

namespace {
int frame = 0;
bool space_held() { return frame >= 2 && frame <= 4; }
}

namespace raylib {
bool IsKeyDown(int key) { return key == KEY_SPACE && space_held(); }
bool IsKeyPressed(int key) { return key == KEY_SPACE && frame == 2; }
}

enum class Action { Left, Right, Drop };

int main() {
  input::add_singleton_components(
      EntityHelper::createEntity(),
      std::map<Action, input::ValidInputs>{
          {Action::Left, {raylib::KEY_LEFT}},
          {Action::Right, {raylib::KEY_RIGHT}},
          {Action::Drop, {raylib::KEY_SPACE}},
      });

  Checks checks;
  auto systems = update_systems<input::InputSystem>();

  int drops = 0;
  int held_frames = 0;
  for (frame = 0; frame < 6; ++frame) {
    systems.run(1.f / 60.f);
    held_frames += input::is_held(Action::Drop);
    drops += input::is_pressed(Action::Drop);
  }
  std::printf("space held %d frames, dropped %d time\n", held_frames, drops);
  checks.check(held_frames == 3, "space held for 3 frames");
  checks.check(drops == 1, "space dropped once");
  return checks.finish("enum_actions");
}
