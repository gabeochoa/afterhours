#include <cstdio>

#include "../../../example_helper.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
using namespace afterhours::examples;

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

  Checks checks;
  auto systems = update_systems<input::InputSystem>();

  run_frames(systems, 1);
  const std::string ctrl_z = fired();
  shift = true;
  run_frames(systems, 1);
  const std::string ctrl_shift_z = fired();

  std::printf("Ctrl+Z fires %s, Ctrl+Shift+Z fires %s\n", ctrl_z.c_str(),
              ctrl_shift_z.c_str());
  auto collector = input::get_input_collector();
  checks.check(ctrl_z == "Undo", "Ctrl+Z fires Undo");
  checks.check(ctrl_shift_z == "Redo", "Ctrl+Shift+Z fires Redo");
  checks.check(collector.inputs_pressed().size() == 1, "one press recorded");
  return checks.finish("chord_specificity");
}
