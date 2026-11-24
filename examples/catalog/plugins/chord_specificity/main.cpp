#include <cstdio>
#include <string>

#include "../../../../ah.h"
#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
namespace inj = testing::input_injector;

enum Action { Undo, Redo };

static std::string fired() {
  auto collector = input::get_input_collector();
  for (const auto &done : collector.inputs_pressed())
    return done.action == Undo ? "Undo" : "Redo";
  return "nothing";
}

static void press_z(bool shift) {
  inj::reset_all();
  inj::set_key_down(keys::LEFT_CONTROL);
  if (shift) inj::set_key_down(keys::LEFT_SHIFT);
  inj::set_key_down(keys::Z);
  inj::reset_frame();
}

int main() {
  using Chord = input::KeyChord;
  testing::test_input::detail::test_mode = true;
  input::add_singleton_components(
      EntityHelper::createPermanentEntity(),
      {{Undo, {Chord{keys::Z, Chord::MOD_CTRL}}},
       {Redo, {Chord{keys::Z, Chord::MOD_CTRL | Chord::MOD_SHIFT}}}});

  SystemManager systems;
  systems.register_update_system(std::make_unique<input::InputSystem>());

  press_z(false);
  systems.run(1.f / 60.f);
  const std::string ctrl_z = fired();
  const auto pressed_after_ctrl_z =
      input::get_input_collector().inputs_pressed().size();

  press_z(true);
  systems.run(1.f / 60.f);
  const std::string ctrl_shift_z = fired();
  const auto pressed_after_ctrl_shift_z =
      input::get_input_collector().inputs_pressed().size();

  std::printf("Ctrl+Z fires %s, Ctrl+Shift+Z fires %s\n", ctrl_z.c_str(),
              ctrl_shift_z.c_str());
  inj::reset_all();
  return ctrl_z == "Undo" && ctrl_shift_z == "Redo" &&
                 pressed_after_ctrl_z == 1 && pressed_after_ctrl_shift_z == 1
             ? 0
             : 1;
}
