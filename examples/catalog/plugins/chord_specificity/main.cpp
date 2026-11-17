#include <cstdio>

#include "../../../../ah.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;

namespace {
bool shift = false;
}

namespace raylib {
bool IsKeyDown(int key) {
  return key == KEY_LEFT_CONTROL || key == KEY_Z || (shift && key == KEY_LEFT_SHIFT);
}
bool IsKeyPressed(int key) { return key == KEY_Z; }
}

enum Action { Undo, Redo };

const char *fired() {
  auto collector = input::get_input_collector();
  for (const auto &done : collector.inputs_pressed())
    return done.action == Undo ? "Undo" : "Redo";
  return "nothing";
}

int main() {
  using Chord = input::KeyChord;
  input::add_singleton_components(
      EntityHelper::createEntity(),
      {{Undo, {Chord{raylib::KEY_Z, Chord::MOD_CTRL}}},
       {Redo, {Chord{raylib::KEY_Z, Chord::MOD_CTRL | Chord::MOD_SHIFT}}}});

  SystemManager systems;
  systems.register_update_system(std::make_unique<input::InputSystem>());

  systems.run(1.f / 60.f);
  const std::string ctrl_z = fired();
  shift = true;
  systems.run(1.f / 60.f);
  const std::string ctrl_shift_z = fired();

  std::printf("Ctrl+Z fires %s, Ctrl+Shift+Z fires %s\n", ctrl_z.c_str(),
              ctrl_shift_z.c_str());
  auto collector = input::get_input_collector();
  return ctrl_z == "Undo" && ctrl_shift_z == "Redo" &&
                 collector.inputs_pressed().size() == 1
             ? 0
             : 1;
}
