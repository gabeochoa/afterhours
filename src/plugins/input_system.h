
#pragma once

#include <map>
#include <variant>

#include "../base_component.h"
#include "../entity_query.h"
#include "../system.h"
#include "developer.h"

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
const int MAX_GAMEPAD_ID = 8;

#ifdef AFTER_HOURS_USE_RAYLIB
using KeyCode = int;
using GamepadID = int;
using GamepadAxis = raylib::GamepadAxis;
using GamepadButton = raylib::GamepadButton;
bool is_gamepad_available(GamepadID id) {
  return raylib::IsGamepadAvailable(id);
}
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

void draw_text(const std::string &text, int x, int y, int fontSize) {
  raylib::DrawText(text.c_str(), x, y, fontSize, raylib::RED);
}

#else
using KeyCode = int;
using GamepadID = int;
using GamepadAxis = int;
using GamepadButton = int;

// TODO good luck ;)
bool is_key_pressed(KeyCode) { return false; }
bool is_key_down(KeyCode) { return false; }
bool is_gamepad_available(GamepadID) { return false; }
float get_gamepad_axis_mvt(GamepadID, GamepadAxis) { return 0.f; }
bool is_gamepad_button_pressed(GamepadID, GamepadButton) { return false; }
bool is_gamepad_button_down(GamepadID, GamepadButton) { return false; }
void draw_text(const std::string &, int, int, int) {}
#endif

enum DeviceMedium {
  Keyboard,
  Gamepad,
  GamepadWithAxis,
};

template <typename T> struct ActionDone {
  GamepadID id;
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

float visit_axis(GamepadID id, GamepadAxisWithDir axis_with_dir) {
  // Note: this one is a bit more complex because we have to check if you
  // are pushing in the right direction while also checking the magnitude
  float mvt = get_gamepad_axis_mvt(id, axis_with_dir.axis);
  // Note: The 0.25 is how big the deadzone is
  // TODO consider making the deadzone configurable?
  if (util::sgn(mvt) == axis_with_dir.dir && abs(mvt) > DEADZONE) {
    return abs(mvt);
  }
  return 0.f;
}

float visit_button(GamepadID id, GamepadButton button) {
  return is_gamepad_button_pressed(id, button) ? 1.f : 0.f;
}

float visit_button_down(GamepadID id, GamepadButton button) {
  return is_gamepad_button_down(id, button) ? 1.f : 0.f;
}

template <typename Action> struct InputCollector : public BaseComponent {
  std::vector<input::ActionDone<Action>> inputs;
  float since_last_input = 0.f;
};

struct ProvidesMaxGamepadID : public BaseComponent {
  int max_gamepad_available = 0;
};

template <typename Action> struct ProvidesInputMapping : public BaseComponent {
  using GameMapping = std::map<Action, input::ValidInputs>;
  GameMapping mapping;
  ProvidesInputMapping(GameMapping start_mapping) : mapping(start_mapping) {}
};

struct RenderConnectedGamepads : System<input::ProvidesMaxGamepadID> {

  virtual void for_each_with(const Entity &,
                             const input::ProvidesMaxGamepadID &mxGamepadID,
                             float) const override {
    draw_text(("Gamepads connected: " +
               std::to_string(mxGamepadID.max_gamepad_available)),
              400, 60, 20);
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
template <typename Action>
struct InputSystem : System<InputCollector<Action>, ProvidesMaxGamepadID,
                            ProvidesInputMapping<Action>> {

  int fetch_max_gampad_id() {
    int result = -1;
    int i = 0;
    while (i < MAX_GAMEPAD_ID) {
      bool avail = is_gamepad_available(i);
      if (!avail) {
        result = i - 1;
        break;
      }
      i++;
    }
    return result;
  }

  float check_single_action(GamepadID id, input::ValidInputs valid_inputs) {
    float value = 0.f;
    for (auto &input : valid_inputs) {
      // value = std::fmax(value,      //
      //              std::visit( //
      //                  util::overloaded{
      //                      //
      //                      [](int keycode) { return visit_key_down(keycode); },
      //                      [id](GamepadAxisWithDir axis_with_dir) {
      //                        return visit_axis(id, axis_with_dir);
      //                      },
      //                      [id](GamepadButton button) {
      //                        return visit_button_down(id, button);
      //                      },
      //                      [](auto) {}},
      //                  input));
    }
    return value;
  }

  virtual void for_each_with(Entity &, InputCollector<Action> &collector,
                             ProvidesMaxGamepadID &mxGamepadID,
                             ProvidesInputMapping<Action> &input_mapper,
                             float dt) override {
    mxGamepadID.max_gamepad_available = std::max(0, fetch_max_gampad_id());
    collector.inputs.clear();

    for (auto &kv : input_mapper.mapping) {
      Action action = kv.first;
      ValidInputs vis = kv.second;

      int i = 0;
      do {
        float amount = check_single_action(i, vis);
        if (amount > 0.f) {
          collector.inputs.push_back(ActionDone{.id = i,
                                                .action = action,
                                                .amount_pressed = 1.f,
                                                .length_pressed = dt});
        }
        i++;
      } while (i <= mxGamepadID.max_gamepad_available);
    }

    if (collector.inputs.size() == 0) {
      collector.since_last_input += dt;
    } else {
      collector.since_last_input = 0;
    }
  }
};

template <typename Action>
void add_singleton_components(
    Entity &entity,
    typename input::ProvidesInputMapping<Action>::GameMapping inital_mapping) {
  entity.addComponent<InputCollector<Action>>();
  entity.addComponent<input::ProvidesMaxGamepadID>();
  entity.addComponent<input::ProvidesInputMapping<Action>>(inital_mapping);
}

template <typename Action> void enforce_singletons(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<InputCollector<Action>>>());
  sm.register_update_system(
      std::make_unique<developer::EnforceSingleton<ProvidesMaxGamepadID>>());
  sm.register_update_system(
      std::make_unique<
          developer::EnforceSingleton<ProvidesInputMapping<Action>>>());
}

template <typename Action> void register_update_systems(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<afterhours::input::InputSystem<Action>>());
}

} // namespace input
} // namespace afterhours
