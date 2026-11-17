#include <cstdio>

#include "../../../../src/plugins/input_system.h"

using namespace afterhours;
using Chord = input::KeyChord;

enum Action { Jump, Fire };

static const char *name(int action) { return action == Jump ? "Jump" : "Fire"; }

static void rebind(input::ProvidesInputMapping &mapping, Action action,
                   const input::AnyInput &key) {
  for (int other : mapping.actions_using(key)) {
    std::printf("%s already uses that key; unbinding it for %s\n", name(other),
                name(action));
    mapping.clear_binding(other);
  }
  mapping.set_binding(action, {key});
}

int main() {
  input::ProvidesInputMapping mapping(
      {{Jump, {Chord{keys::SPACE}}}, {Fire, {Chord{keys::ENTER}}}});

  rebind(mapping, Fire, Chord{keys::SPACE});

  const bool fire_on_space =
      mapping.actions_using(Chord{keys::SPACE}) == std::vector<int>{Fire};
  std::printf("Space now fires: %s; Jump bound: %s\n",
              fire_on_space ? "Fire" : "something else",
              mapping.mapping.contains(Jump) ? "yes" : "no");
  return fire_on_space && !mapping.mapping.contains(Jump) ? 0 : 1;
}
