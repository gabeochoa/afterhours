#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include <cassert>

using afterhours::input;
using Chord = input::KeyChord;

enum Action { Undo, Redo, Type, Save };

static input::ActionDone pressed(Action action, int key, uint8_t mods, bool explicit_mods = true) {
  return input::ActionDone(input::DeviceMedium::Keyboard, 0, action, 1.f, 0.f,
                           key, mods, explicit_mods);
}

static bool fired(const std::vector<input::ActionDone> &done, Action action) {
  for (const auto &d : done)
    if (d.action == action) return true;
  return false;
}

int main() {
  const int Z = 'Z', S = 'S';
  const uint8_t ctrl = Chord::MOD_CTRL, ctrl_shift = Chord::MOD_CTRL | Chord::MOD_SHIFT;

  std::vector<input::ActionDone> both = {pressed(Undo, Z, ctrl), pressed(Redo, Z, ctrl_shift)};
  input::suppress_permissive_duplicates(both);
  assert(!fired(both, Undo) && fired(both, Redo));

  std::vector<input::ActionDone> undo_only = {pressed(Undo, Z, ctrl)};
  input::suppress_permissive_duplicates(undo_only);
  assert(fired(undo_only, Undo));

  std::vector<input::ActionDone> bare = {pressed(Type, Z, 0, false), pressed(Undo, Z, ctrl)};
  input::suppress_permissive_duplicates(bare);
  assert(!fired(bare, Type) && fired(bare, Undo));

  std::vector<input::ActionDone> other_key = {pressed(Save, S, ctrl), pressed(Redo, Z, ctrl_shift)};
  input::suppress_permissive_duplicates(other_key);
  assert(fired(other_key, Save) && fired(other_key, Redo));

  // Backend-free: the injected key reader must control modifiers too, or a
  // unit test can never prove Cmd+B fires only while Cmd is held.
  auto no_buttons = [](input::GamepadID, input::GamepadButton) { return 0.f; };
  auto cmd_held = [](int key) {
    return key == afterhours::keys::LEFT_SUPER || key == 'B';
  };
  input::ValidInputs cmd_b = {input::AnyInput{Chord{'B', Chord::MOD_SUPER}}};
  auto hit = input::check_single_action_impl(0, cmd_b, cmd_held, no_buttons);
  assert(hit.value == 1.f && hit.matched_key == 'B');

  auto cmd_released = [](int key) { return key == 'B'; };
  auto miss = input::check_single_action_impl(0, cmd_b, cmd_released, no_buttons);
  assert(miss.value == 0.f);
  return 0;
}
