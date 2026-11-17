#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include <cassert>

using namespace afterhours;
using Chord = input::KeyChord;

enum Action { Jump, Fire, Pause };
enum class Layer { Game, Menu };

int main() {
  const input::AnyInput space = Chord{keys::SPACE};
  const input::AnyInput enter = Chord{keys::ENTER};
  const input::AnyInput ctrl_space = Chord{keys::SPACE, Chord::MOD_CTRL};
  const input::AnyInput button = input::GamepadButton{3};

  input::ProvidesInputMapping mapping({{Jump, {space, button}}, {Fire, {enter}}});
  assert(mapping.actions_using(space) == std::vector<int>{Jump});
  assert(mapping.actions_using(button) == std::vector<int>{Jump});
  assert(mapping.actions_using(ctrl_space).empty());

  mapping.set_binding(Fire, {space});
  assert((mapping.actions_using(space) == std::vector<int>{Jump, Fire}));
  assert(mapping.actions_using(enter).empty());

  mapping.clear_binding(Jump);
  assert(mapping.actions_using(space) == std::vector<int>{Fire});
  assert(!mapping.mapping.contains(Jump));

  ProvidesLayeredInputMapping<Layer> layered;
  layered.set_binding(Layer::Menu, Pause, {space});
  assert(layered.actions_using(Layer::Menu, space) == std::vector<int>{Pause});
  assert(layered.actions_using(Layer::Game, space).empty());
  return 0;
}
