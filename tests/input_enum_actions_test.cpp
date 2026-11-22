#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include "input_raylib_unused_stubs.h"
#include <cassert>

namespace {
bool space_down = false, space_edge = false;
float stick_x = 0;
}
namespace raylib {
Vector2 GetMouseDelta() { return {}; }
bool IsGamepadAvailable(int id) { return id <= 1; }
bool IsKeyPressed(int key) { return key == afterhours::keys::SPACE && space_edge; }
bool IsKeyPressedRepeat(int) { return false; }
bool IsKeyDown(int key) { return key == afterhours::keys::SPACE && space_down; }
bool IsMouseButtonDown(int) { return false; }
bool IsGamepadButtonPressed(int, int) { return false; }
bool IsGamepadButtonDown(int, int) { return false; }
float GetGamepadAxisMovement(int pad, int axis) { return pad == 1 && axis == 0 ? stick_x : 0; }
}

enum class Action { Left, Drop };

int main() {
  using namespace afterhours;
  assert(!input::is_held(Action::Drop));

  Entity &entity = EntityHelper::createPermanentEntity();
  input::add_singleton_components(
      entity, std::map<Action, input::ValidInputs>{
                  {Action::Left, {input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(0), -1}}},
                  {Action::Drop, {input::KeyChord{keys::SPACE}}}});
  input::InputSystem system;
  auto frame = [&] {
    system.for_each_with(entity, entity.get<input::InputCollector>(),
                         entity.get<input::ProvidesMaxGamepadID>(),
                         entity.get<input::ProvidesInputMapping>(), 1.f / 60.f);
  };

  frame();
  assert(!input::is_held(Action::Drop) && !input::is_pressed(Action::Drop));

  space_down = space_edge = true;
  frame();
  assert(input::is_held(Action::Drop) && input::is_pressed(Action::Drop));
  assert(!input::is_held(Action::Left));

  space_edge = false;
  frame();
  assert(input::is_held(Action::Drop) && !input::is_pressed(Action::Drop));

  stick_x = -0.9f;
  frame();
  assert(input::is_held(Action::Left));
  return 0;
}
