
#pragma once

#include <map>
#include <variant>

#include "../base_component.h"
#include "../developer.h"
#include "../entity_query.h"
#include "../system.h"

namespace afterhours {

struct input : developer::Plugin {

  static constexpr float DEADZONE = 0.25f;
  static constexpr int MAX_GAMEPAD_ID = 8;

  using MouseButton = int;
#ifdef AFTER_HOURS_USE_RAYLIB
  using MousePosition = raylib::Vector2;
  using KeyCode = int;
  using GamepadID = int;
  using GamepadAxis = raylib::GamepadAxis;
  using GamepadButton = raylib::GamepadButton;

  static MousePosition get_mouse_position() {
    return raylib::GetMousePosition();
  }
  static bool is_mouse_button_up(MouseButton button) {
    return raylib::IsMouseButtonUp(button);
  }
  static bool is_mouse_button_down(MouseButton button) {
    return raylib::IsMouseButtonDown(button);
  }
  static bool is_mouse_button_pressed(MouseButton button) {
    return raylib::IsMouseButtonPressed(button);
  }
  static bool is_mouse_button_released(MouseButton button) {
    return raylib::IsMouseButtonReleased(button);
  }
  static bool is_gamepad_available(GamepadID id) {
    return raylib::IsGamepadAvailable(id);
  }
  static bool is_key_pressed(KeyCode keycode) {
    return raylib::IsKeyPressed(keycode);
  }
  static bool is_key_down(KeyCode keycode) {
    return raylib::IsKeyDown(keycode);
  }
  static float get_gamepad_axis_mvt(GamepadID gamepad_id, GamepadAxis axis) {
    return raylib::GetGamepadAxisMovement(gamepad_id, axis);
  }
  static bool is_gamepad_button_pressed(GamepadID gamepad_id,
                                        GamepadButton button) {
    return raylib::IsGamepadButtonPressed(gamepad_id, button);
  }
  static bool is_gamepad_button_down(GamepadID gamepad_id,
                                     GamepadButton button) {
    return raylib::IsGamepadButtonDown(gamepad_id, button);
  }

  static void draw_text(const std::string &text, int x, int y, int fontSize) {
    raylib::DrawText(text.c_str(), x, y, fontSize, raylib::RED);
  }

  static void set_gamepad_mappings(const std::string &data) {
    raylib::SetGamepadMappings(data.c_str());
  }

  static std::string name_for_button(GamepadButton input) {
    switch (input) {
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_UP:
      return "D-Pad Up";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_RIGHT:
      return "D-Pad Right";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN:
      return "D-Pad Down";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_LEFT:
      return "D-Pad Left";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_UP:
      return "PS3: Triangle, Xbox: Y";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:
      return "PS3: Square, Xbox: X";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_DOWN:
      return "PS3: Cross, Xbox: A";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_LEFT:
      return "PS3: Circle, Xbox: B";
    case raylib::GAMEPAD_BUTTON_MIDDLE_LEFT:
      return "Select";
    case raylib::GAMEPAD_BUTTON_MIDDLE:
      return "PS3: PS, Xbox: XBOX";
    case raylib::GAMEPAD_BUTTON_MIDDLE_RIGHT:
      return "Start";
    default:
      return std::string(magic_enum::enum_name(input));
    }
  }

  static std::string icon_for_button(GamepadButton input) {
    switch (input) {
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_UP:
      return "xbox_dpad_up";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_RIGHT:
      return "xbox_dpad_right";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_DOWN:
      return "xbox_dpad_down";
    case raylib::GAMEPAD_BUTTON_LEFT_FACE_LEFT:
      return "xbox_dpad_left";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_UP:
      // return "PS3: Triangle, Xbox: Y";
      // TODO for now just use the xbox ones
      return "xbox_button_color_y";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_LEFT:
      // return "PS3: Square, Xbox: X";
      return "xbox_button_color_x";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_DOWN:
      // return "PS3: Cross, Xbox: A";
      return "xbox_button_color_a";
    case raylib::GAMEPAD_BUTTON_RIGHT_FACE_RIGHT:
      // return "PS3: Circle, Xbox: B";
      return "xbox_button_color_b";
    case raylib::GAMEPAD_BUTTON_MIDDLE_LEFT:
      return "xbox_button_view";
    case raylib::GAMEPAD_BUTTON_MIDDLE:
      return "xbox_guide";
    case raylib::GAMEPAD_BUTTON_MIDDLE_RIGHT:
      return "xbox_button_menu";
    default:
      log_warn("Missing icon for button {}", magic_enum::enum_name(input));
      return std::string(magic_enum::enum_name(input));
    }
  }

  static std::string icon_for_key(int keycode) {
    raylib::KeyboardKey key =
        magic_enum::enum_cast<raylib::KeyboardKey>(keycode).value_or(
            raylib::KeyboardKey::KEY_NULL);
    switch (key) {
    case raylib::KEY_TAB:
      return "keyboard_tab";
    case raylib::KEY_LEFT_SHIFT:
      return "keyboard_shift";
    case raylib::KEY_BACKSPACE:
      return "keyboard_backspace";
    case raylib::KEY_LEFT_SUPER:
      // TODO add icon for mac?
      return "keyboard_win";
    case raylib::KEY_V:
      return "keyboard_v";
    case raylib::KEY_ENTER:
      return "keyboard_enter";
    case raylib::KEY_UP:
      return "keyboard_arrow_up";
    case raylib::KEY_DOWN:
      return "keyboard_arrow_down";
    case raylib::KEY_LEFT:
      return "keyboard_arrow_left";
    case raylib::KEY_RIGHT:
      return "keyboard_arrow_right";
    case raylib::KEY_W:
      return "keyboard_w";
    case raylib::KEY_S:
      return "keyboard_s";
    case raylib::KEY_SPACE:
      return "keyboard_space";
    case raylib::KEY_R:
      return "keyboard_r";
    case raylib::KEY_BACKSLASH:
      return "keyboard_slash_back";
    case raylib::KEY_L:
      return "keyboard_l";
    case raylib::KEY_EQUAL:
      return "keyboard_equals";
    case raylib::KEY_A:
      return "keyboard_a";
    case raylib::KEY_B:
      return "keyboard_b";
    case raylib::KEY_C:
      return "keyboard_c";
    case raylib::KEY_D:
      return "keyboard_d";
    case raylib::KEY_E:
      return "keyboard_e";
    case raylib::KEY_F:
      return "keyboard_f";
    case raylib::KEY_G:
      return "keyboard_g";
    case raylib::KEY_H:
      return "keyboard_h";
    case raylib::KEY_APOSTROPHE:
      return "keyboard_apostrophe";
    case raylib::KEY_COMMA:
      return "keyboard_comma";
    case raylib::KEY_MINUS:
      return "keyboard_minus";
    case raylib::KEY_PERIOD:
      return "keyboard_period";
    case raylib::KEY_SLASH:
      return "keyboard_slash";
    case raylib::KEY_ZERO:
      return "keyboard_zero";
    case raylib::KEY_ONE:
      return "keyboard_one";
    case raylib::KEY_TWO:
      return "keyboard_two";
    case raylib::KEY_THREE:
      return "keyboard_three";
    case raylib::KEY_FOUR:
      return "keyboard_four";
    case raylib::KEY_FIVE:
      return "keyboard_five";
    case raylib::KEY_SIX:
      return "keyboard_six";
    case raylib::KEY_SEVEN:
      return "keyboard_seven";
    case raylib::KEY_EIGHT:
      return "keyboard_eight";
    case raylib::KEY_NINE:
      return "keyboard_nine";
    case raylib::KEY_SEMICOLON:
      return "keyboard_semicolon";
    case raylib::KEY_I:
      return "keyboard_i";
    case raylib::KEY_J:
      return "keyboard_j";
    case raylib::KEY_K:
      return "keyboard_k";
    case raylib::KEY_M:
      return "keyboard_m";
    case raylib::KEY_N:
      return "keyboard_n";
    case raylib::KEY_O:
      return "keyboard_o";
    case raylib::KEY_P:
      return "keyboard_p";
    case raylib::KEY_Q:
      return "keyboard_q";
    case raylib::KEY_T:
      return "keyboard_t";
    case raylib::KEY_U:
      return "keyboard_u";
    case raylib::KEY_X:
      return "keyboard_x";
    case raylib::KEY_Y:
      return "keyboard_y";
    case raylib::KEY_Z:
      return "keyboard_z";
    case raylib::KEY_LEFT_BRACKET:
      return "keyboard_left_bracket";
    case raylib::KEY_RIGHT_BRACKET:
      return "keyboard_right_bracket";
    case raylib::KEY_GRAVE:
      return "keyboard_grave";
    case raylib::KEY_ESCAPE:
      return "keyboard_escape";
    case raylib::KEY_INSERT:
      return "keyboard_insert";
    case raylib::KEY_DELETE:
      return "keyboard_delete";
    case raylib::KEY_PAGE_UP:
      return "keyboard_page_up";
    case raylib::KEY_PAGE_DOWN:
      return "keyboard_page_down";
    case raylib::KEY_HOME:
      return "keyboard_home";
    case raylib::KEY_END:
      return "keyboard_end";
    case raylib::KEY_CAPS_LOCK:
      return "keyboard_caps_lock";
    case raylib::KEY_SCROLL_LOCK:
      return "keyboard_scroll_lock";
    case raylib::KEY_NUM_LOCK:
      return "keyboard_num_lock";
    case raylib::KEY_PRINT_SCREEN:
      return "keyboard_print_screen";
    case raylib::KEY_PAUSE:
      return "keyboard_pause";
    case raylib::KEY_F1:
      return "keyboard_f1";
    case raylib::KEY_F2:
      return "keyboard_f2";
    case raylib::KEY_F3:
      return "keyboard_f3";
    case raylib::KEY_F4:
      return "keyboard_f4";
    case raylib::KEY_F5:
      return "keyboard_f5";
    case raylib::KEY_F6:
      return "keyboard_f6";
    case raylib::KEY_F7:
      return "keyboard_f7";
    case raylib::KEY_F8:
      return "keyboard_f8";
    case raylib::KEY_F9:
      return "keyboard_f9";
    case raylib::KEY_F10:
      return "keyboard_f10";
    case raylib::KEY_F11:
      return "keyboard_f11";
    case raylib::KEY_F12:
      return "keyboard_f12";
    case raylib::KEY_LEFT_CONTROL:
      return "keyboard_left_control";
    case raylib::KEY_LEFT_ALT:
      return "keyboard_left_alt";
    case raylib::KEY_RIGHT_SHIFT:
      return "keyboard_right_shift";
    case raylib::KEY_RIGHT_CONTROL:
      return "keyboard_right_control";
    case raylib::KEY_RIGHT_ALT:
      return "keyboard_right_alt";
    case raylib::KEY_RIGHT_SUPER:
      return "keyboard_right_super";
    case raylib::KEY_KB_MENU:
      return "keyboard_kb_menu";
    case raylib::KEY_KP_0:
      return "keyboard_kp_0";
    case raylib::KEY_KP_1:
      return "keyboard_kp_1";
    case raylib::KEY_KP_2:
      return "keyboard_kp_2";
    case raylib::KEY_KP_3:
      return "keyboard_kp_3";
    case raylib::KEY_KP_4:
      return "keyboard_kp_4";
    case raylib::KEY_KP_5:
      return "keyboard_kp_5";
    case raylib::KEY_KP_6:
      return "keyboard_kp_6";
    case raylib::KEY_KP_7:
      return "keyboard_kp_7";
    case raylib::KEY_KP_8:
      return "keyboard_kp_8";
    case raylib::KEY_KP_9:
      return "keyboard_kp_9";
    case raylib::KEY_KP_DECIMAL:
      return "keyboard_kp_decimal";
    case raylib::KEY_KP_DIVIDE:
      return "keyboard_kp_divide";
    case raylib::KEY_KP_MULTIPLY:
      return "keyboard_kp_multiply";
    case raylib::KEY_KP_SUBTRACT:
      return "keyboard_kp_subtract";
    case raylib::KEY_KP_ADD:
      return "keyboard_kp_add";
    case raylib::KEY_KP_ENTER:
      return "keyboard_kp_enter";
    case raylib::KEY_KP_EQUAL:
      return "keyboard_kp_equal";
    case raylib::KEY_BACK:
      return "keyboard_back";
    case raylib::KEY_VOLUME_UP:
      return "keyboard_volume_up";
    case raylib::KEY_VOLUME_DOWN:
      return "keyboard_volume_down";
    case raylib::KEY_MENU:
      return "keyboard_menu";
    case raylib::KEY_NULL:
      log_info("Passed in {} but wasnt able to parse it", keycode);
      break;
    }
    return "";
  }

#else
  using MousePosition = MyVec2;
  using KeyCode = int;
  using GamepadID = int;
  using GamepadAxis = int;
  using GamepadButton = int;

  // TODO good luck ;)
  static MousePosition get_mouse_position() { return {0, 0}; }
  static bool is_mouse_button_up(MouseButton) { return false; }
  static bool is_mouse_button_down(MouseButton) { return false; }
  static bool is_mouse_button_pressed(MouseButton) { return false; }
  static bool is_mouse_button_released(MouseButton) { return false; }
  static

      bool
      is_key_pressed(KeyCode) {
    return false;
  }
  static bool is_key_down(KeyCode) { return false; }
  static bool is_gamepad_available(GamepadID) { return false; }
  static float get_gamepad_axis_mvt(GamepadID, GamepadAxis) { return 0.f; }
  static bool is_gamepad_button_pressed(GamepadID, GamepadButton) {
    return false;
  }
  static bool is_gamepad_button_down(GamepadID, GamepadButton) { return false; }
  static void draw_text(const std::string &, int, int, int) {}
  static void set_gamepad_mappings(const std::string &) {}
  static std::string name_for_button(GamepadButton) { return "unknown"; }
  static std::string icon_for_button(GamepadButton) { return "unknown"; }
  static std::string icon_for_key(int) { return "unknown"; }
#endif

  enum struct DeviceMedium {
    None,
    Keyboard,
    GamepadButton,
    GamepadAxis,
  };

  template <typename T> struct ActionDone {
    DeviceMedium medium;
    GamepadID id;
    T action;
    float amount_pressed;
    float length_pressed;
  };

  struct GamepadAxisWithDir {
    GamepadAxis axis;
    int dir = -1;
  };

  using AnyInput = std::variant<KeyCode, GamepadAxisWithDir, GamepadButton>;
  using ValidInputs = std::vector<AnyInput>;

  static float visit_key(int keycode) {
    return is_key_pressed(keycode) ? 1.f : 0.f;
  }
  static float visit_key_down(int keycode) {
    return is_key_down(keycode) ? 1.f : 0.f;
  }

  static float visit_axis(GamepadID id, GamepadAxisWithDir axis_with_dir) {
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

  static float visit_button(GamepadID id, GamepadButton button) {
    return is_gamepad_button_pressed(id, button) ? 1.f : 0.f;
  }

  static float visit_button_down(GamepadID id, GamepadButton button) {
    return is_gamepad_button_down(id, button) ? 1.f : 0.f;
  }

  template <typename Action> struct InputCollector : public BaseComponent {
    std::vector<input::ActionDone<Action>> inputs;
    std::vector<input::ActionDone<Action>> inputs_pressed;
    float since_last_input = 0.f;
  };

  struct ProvidesMaxGamepadID : public BaseComponent {
    int max_gamepad_available = 0;
    size_t count() const { return (size_t)max_gamepad_available + 1; }
  };

  template <typename Action>
  struct ProvidesInputMapping : public BaseComponent {
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

    [[nodiscard]] std::vector<input::ActionDone<Action>> &inputs_pressed() {
      return data.value().get().inputs_pressed;
    }

    [[nodiscard]] float since_last_input() {
      return data.value().get().since_last_input;
    }
  };

  template <typename Action>
  static auto get_input_collector() -> PossibleInputCollector<Action> {

    // TODO replace with a singletone query
    OptEntity opt_collector =
        EntityQuery().whereHasComponent<InputCollector<Action>>().gen_first();
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
      while (i < ::afterhours::input::MAX_GAMEPAD_ID) {
        bool avail = is_gamepad_available(i);
        if (!avail) {
          result = i - 1;
          break;
        }
        i++;
      }
      return result;
    }

    std::pair<DeviceMedium, float>
    check_single_action_pressed(GamepadID id, input::ValidInputs valid_inputs) {
      DeviceMedium medium;
      float value = 0.f;
      for (auto &input : valid_inputs) {
        DeviceMedium temp_medium = DeviceMedium::None;
        float temp =             //
            std::visit(          //
                util::overloaded{//
                                 [&](int keycode) {
                                   temp_medium = DeviceMedium::Keyboard;
                                   return is_key_pressed(keycode) ? 1.f : 0.f;
                                 },
                                 [&](GamepadAxisWithDir axis_with_dir) {
                                   temp_medium = DeviceMedium::GamepadAxis;
                                   return visit_axis(id, axis_with_dir);
                                 },
                                 [&](GamepadButton button) {
                                   temp_medium = DeviceMedium::GamepadButton;
                                   return is_gamepad_button_pressed(id, button)
                                              ? 1.f
                                              : 0.f;
                                 },
                                 [](auto) {}},
                input);
        if (temp > value) {
          value = temp;
          medium = temp_medium;
        }
      }
      return {medium, value};
    }

    std::pair<DeviceMedium, float>
    check_single_action_down(GamepadID id, input::ValidInputs valid_inputs) {
      DeviceMedium medium;
      float value = 0.f;
      for (auto &input : valid_inputs) {
        DeviceMedium temp_medium = DeviceMedium::None;
        float temp =             //
            std::visit(          //
                util::overloaded{//
                                 [&](int keycode) {
                                   temp_medium = DeviceMedium::Keyboard;
                                   return visit_key_down(keycode);
                                 },
                                 [&](GamepadAxisWithDir axis_with_dir) {
                                   temp_medium = DeviceMedium::GamepadAxis;
                                   return visit_axis(id, axis_with_dir);
                                 },
                                 [&](GamepadButton button) {
                                   temp_medium = DeviceMedium::GamepadButton;
                                   return visit_button_down(id, button);
                                 },
                                 [](auto) {}},
                input);
        if (temp > value) {
          value = temp;
          medium = temp_medium;
        }
      }
      return {medium, value};
    }

    virtual void for_each_with(Entity &, InputCollector<Action> &collector,
                               ProvidesMaxGamepadID &mxGamepadID,
                               ProvidesInputMapping<Action> &input_mapper,
                               float dt) override {
      mxGamepadID.max_gamepad_available = std::max(0, fetch_max_gampad_id());
      collector.inputs.clear();
      collector.inputs_pressed.clear();

      for (auto &kv : input_mapper.mapping) {
        Action action = kv.first;
        ValidInputs vis = kv.second;

        int i = 0;
        do {
          // down
          {
            auto [medium, amount] = check_single_action_down(i, vis);
            if (amount > 0.f) {
              collector.inputs.push_back(ActionDone{.medium = medium,
                                                    .id = i,
                                                    .action = action,
                                                    .amount_pressed = amount,
                                                    .length_pressed = dt});
            }
          }
          // pressed
          {
            auto [medium, amount] = check_single_action_pressed(i, vis);
            if (amount > 0.f) {
              collector.inputs_pressed.push_back(
                  ActionDone{.medium = medium,
                             .id = i,
                             .action = action,
                             .amount_pressed = amount,
                             .length_pressed = dt});
            }
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
  static void add_singleton_components(
      Entity &entity, typename input::ProvidesInputMapping<Action>::GameMapping
                          inital_mapping) {
    entity.addComponent<InputCollector<Action>>();
    entity.addComponent<input::ProvidesMaxGamepadID>();
    entity.addComponent<input::ProvidesInputMapping<Action>>(inital_mapping);

    EntityHelper::registerSingleton<InputCollector<Action>>(entity);
    EntityHelper::registerSingleton<input::ProvidesMaxGamepadID>(entity);
    EntityHelper::registerSingleton<input::ProvidesInputMapping<Action>>(
        entity);
  }

  template <typename Action> static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<
            developer::EnforceSingleton<InputCollector<Action>>>());
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesMaxGamepadID>>());
    sm.register_update_system(
        std::make_unique<
            developer::EnforceSingleton<ProvidesInputMapping<Action>>>());
  }

  template <typename Action>
  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<afterhours::input::InputSystem<Action>>());
  }

  // Renderer Systems:
  // RenderConnectedGamepads
};
} // namespace afterhours
