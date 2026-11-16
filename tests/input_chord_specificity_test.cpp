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
  return 0;
}
