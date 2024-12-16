
#pragma once

#include <map>
#include <variant>

#include "../base_component.h"
#include "../entity_query.h"
#include "../system.h"

/*




*/
namespace afterhours {

// TODO move into a dedicated file?
namespace util {
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};

template <typename T> int sgn(T val) { return (T(0) < val) - (val < T(0)); }
} // namespace util

namespace input {

float DEADZONE = 0.25f;

#ifdef AFTER_HOURS_USE_RAYLIB
using KeyCode = int;
using GamepadID = int;
using GamepadAxis = raylib::GamepadAxis;
using GamepadButton = raylib::GamepadButton;
bool is_key_pressed(KeyCode keycode) { return raylib::IsKeyPressed(keycode); }
bool is_key_down(KeyCode keycode) { return raylib::IsKeyDown(keycode); }
float get_gamepad_axis_mvt(GamepadID gamepad_id, GamepadAxis axis) {
  return raylib::GetGamepadAxisMovement(gamepad_id, axis);
}
bool is_gamepad_button_pressed(GamepadID gamepad_id, GamepadButton button) {
  return raylib::IsGamepadButtonPressed(gamepad_id, button);
}
bool is_gamepad_button_down(GamepadID gamepad_id, GamepadButton button) {
  return raylib::IsGamepadButtonDown(gamepad_id, button);
}
#else
using KeyCode = int;
using GamepadID = int;
using GamepadAxis = int;
using GamepadButton = int;

// TODO good luck ;)
bool is_key_pressed(KeyCode) { return false; }
bool is_key_down(KeyCode) { return false; }
float get_gamepad_axis_mvt(GamepadID, GamepadAxis) { return 0.f; }
bool is_gamepad_button_pressed(GamepadID, GamepadButton) { return false; }
bool is_gamepad_button_down(GamepadID, GamepadButton) { return false; }
#endif

enum DeviceMedium {
  Keyboard,
  Gamepad,
  GamepadWithAxis,
};

template <typename T> struct ActionDone {
  T action;
  float amount_pressed;
  float length_pressed;
};

struct GamepadAxisWithDir {
  GamepadAxis axis;
  float dir = -1;
};

using AnyInput = std::variant<KeyCode, GamepadAxisWithDir, GamepadButton>;
using ValidInputs = std::vector<AnyInput>;

float visit_key(int keycode) { return is_key_pressed(keycode) ? 1.f : 0.f; }
float visit_key_down(int keycode) { return is_key_down(keycode) ? 1.f : 0.f; }

float visit_axis(GamepadAxisWithDir axis_with_dir) {
  // Note: this one is a bit more complex because we have to check if you
  // are pushing in the right direction while also checking the magnitude
  float mvt = get_gamepad_axis_mvt(0, axis_with_dir.axis);
  // Note: The 0.25 is how big the deadzone is
  // TODO consider making the deadzone configurable?
  if (util::sgn(mvt) == axis_with_dir.dir && abs(mvt) > DEADZONE) {
    return abs(mvt);
  }
  return 0.f;
}

// TODO support multiple gamepads
float visit_button(GamepadButton button) {
  return is_gamepad_button_pressed(0, button) ? 1.f : 0.f;
}

float visit_button_down(GamepadButton button) {
  return is_gamepad_button_down(0, button) ? 1.f : 0.f;
}

template <typename Action> struct InputCollector : public BaseComponent {
  std::vector<input::ActionDone<Action>> inputs;
  float since_last_input = 0.f;
};

template <typename Action> struct InputSystem : System<InputCollector<Action>> {

  using GameMapping = std::map<Action, input::ValidInputs>;

  //
  GameMapping mapping;
  //

  InputSystem(GameMapping start_mapping) : mapping(start_mapping) {}

  float check_single_action(input::ValidInputs valid_inputs) {
    float value = 0.f;
    for (auto &input : valid_inputs) {
      value = fmax(value,      //
                   std::visit( //
                       util::overloaded{
                           //
                           [](int keycode) { return visit_key_down(keycode); },
                           [](GamepadAxisWithDir axis_with_dir) {
                             return visit_axis(axis_with_dir);
                           },
                           [](GamepadButton button) {
                             return visit_button_down(button);
                           },
                           [](auto) {}},
                       input));
    }
    return value;
  }

  virtual void for_each_with(Entity &, InputCollector<Action> &collector,
                             float dt) override {
    collector.inputs.clear();

    for (auto &kv : mapping) {
      Action action = kv.first;
      ValidInputs vis = kv.second;
      float amount = check_single_action(vis);
      if (amount > 0.f) {
        collector.inputs.push_back(ActionDone{
            .action = action, .amount_pressed = 1.f, .length_pressed = dt});
      }
    }

    if (collector.inputs.size() == 0) {
      collector.since_last_input += dt;
    } else {
      collector.since_last_input = 0;
    }
  }
};

struct InputQuery : EntityQuery<InputQuery> {};
template <typename Action> struct PossibleInputCollector {
  std::optional<std::reference_wrapper<InputCollector<Action>>> data;
  PossibleInputCollector(
      std::optional<std::reference_wrapper<InputCollector<Action>>> d)
      : data(d) {}
  PossibleInputCollector(InputCollector<Action> &d) : data(d) {}
  PossibleInputCollector() : data({}) {}
  bool has_value() const { return data.has_value(); }
  bool valid() const { return has_value(); }

  [[nodiscard]] std::vector<input::ActionDone<Action>> &inputs() {
    return data.value().get().inputs;
  }

  [[nodiscard]] float since_last_input() {
    return data.value().get().since_last_input;
  }
};

template <typename Action>
auto get_input_collector() -> PossibleInputCollector<Action> {

  OptEntity opt_collector =
      InputQuery().whereHasComponent<InputCollector<Action>>().gen_first();
  if (!opt_collector.valid())
    return {};
  Entity &collector = opt_collector.asE();
  return collector.get<InputCollector<Action>>();
}

// TODO i would like to move this out of input namespace
// at some point

} // namespace input
} // namespace afterhours
