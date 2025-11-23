
#pragma once

#include <cmath>
#include <map>
#include <variant>

#include "../core/base_component.h"
#include "../core/entity_query.h"
#include "../core/system.h"
#include "../developer.h"
#include "window_manager.h"

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
        const raylib::Vector2 raw = raylib::GetMousePosition();

        const int window_w = raylib::GetScreenWidth();
        const int window_h = raylib::GetScreenHeight();

        const auto *pcr = EntityHelper::get_singleton_cmp<
            window_manager::ProvidesCurrentResolution>();
        if (pcr == nullptr) {
            return raw;
        }
        const float content_w =
            static_cast<float>(pcr->current_resolution.width);
        const float content_h =
            static_cast<float>(pcr->current_resolution.height);
        if (content_w <= 0.0f || content_h <= 0.0f) {
            return raw;
        }

        int dest_w = window_w;
        int dest_h = static_cast<int>(
            std::round((double) dest_w * content_h / content_w));
        if (dest_h > window_h) {
            dest_h = window_h;
            dest_w = static_cast<int>(
                std::round((double) dest_h * content_w / content_h));
        }
        const int bar_w_total = window_w - dest_w;
        const int bar_h_total = window_h - dest_h;
        const int bar_left = bar_w_total / 2;
        const int bar_top = bar_h_total / 2;

        const float min_x = static_cast<float>(bar_left);
        const float min_y = static_cast<float>(bar_top);
        const float max_x = static_cast<float>(bar_left + dest_w);
        const float max_y = static_cast<float>(bar_top + dest_h);

        if (raw.x < min_x || raw.x > max_x || raw.y < min_y || raw.y > max_y) {
            return raw;
        }

        const float scale_x = content_w / static_cast<float>(dest_w);
        const float scale_y = content_h / static_cast<float>(dest_h);
        return {(raw.x - min_x) * scale_x, (raw.y - min_y) * scale_y};
    }
    static bool is_mouse_button_up(const MouseButton button) {
        return raylib::IsMouseButtonUp(button);
    }
    static bool is_mouse_button_down(const MouseButton button) {
        return raylib::IsMouseButtonDown(button);
    }
    static bool is_mouse_button_pressed(const MouseButton button) {
        return raylib::IsMouseButtonPressed(button);
    }
    static bool is_mouse_button_released(const MouseButton button) {
        return raylib::IsMouseButtonReleased(button);
    }
    static bool is_gamepad_available(const GamepadID id) {
        return raylib::IsGamepadAvailable(id);
    }
    static bool is_key_pressed(const KeyCode keycode) {
        return raylib::IsKeyPressed(keycode);
    }
    static bool is_key_down(const KeyCode keycode) {
        return raylib::IsKeyDown(keycode);
    }
    static float get_gamepad_axis_mvt(const GamepadID gamepad_id,
                                      const GamepadAxis axis) {
        return raylib::GetGamepadAxisMovement(gamepad_id, axis);
    }
    static bool is_gamepad_button_pressed(const GamepadID gamepad_id,
                                          const GamepadButton button) {
        return raylib::IsGamepadButtonPressed(gamepad_id, button);
    }
    static bool is_gamepad_button_down(const GamepadID gamepad_id,
                                       const GamepadButton button) {
        return raylib::IsGamepadButtonDown(gamepad_id, button);
    }

    static void draw_text(const std::string &text, const int x, const int y,
                          const int fontSize) {
        raylib::DrawText(text.c_str(), x, y, fontSize, raylib::RED);
    }

    static void set_gamepad_mappings(const std::string &data) {
        raylib::SetGamepadMappings(data.c_str());
    }

    static std::string name_for_button(const GamepadButton input) {
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

    static std::string icon_for_button(const GamepadButton input) {
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
                log_warn("Missing icon for button {}",
                         magic_enum::enum_name(input));
                return std::string(magic_enum::enum_name(input));
        }
    }

    static std::string icon_for_key(const int keycode) {
        const raylib::KeyboardKey key =
            magic_enum::enum_cast<raylib::KeyboardKey>(
                static_cast<unsigned int>(keycode))
                .value_or(raylib::KeyboardKey::KEY_NULL);
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
                // TODO figure out why this is the same as KEY_R
            case raylib::KEY_MENU:
                // Raylib 5.5 adds KEY_MENU (Android). Use same icon as KB menu.
                return "keyboard_kb_menu";
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

    // Wrapper types to avoid duplicate int in variant
    struct KeyCodeWrapper {
        int value;
    };
    struct GamepadButtonWrapper {
        int value;
    };

    // TODO good luck ;)
    static MousePosition get_mouse_position() { return {0, 0}; }
    static bool is_mouse_button_up(const MouseButton) { return false; }
    static bool is_mouse_button_down(const MouseButton) { return false; }
    static bool is_mouse_button_pressed(const MouseButton) { return false; }
    static bool is_mouse_button_released(const MouseButton) { return false; }
    static

        bool
        is_key_pressed(const KeyCode) {
        return false;
    }
    static bool is_key_down(const KeyCode) { return false; }
    static bool is_gamepad_available(const GamepadID) { return false; }
    static float get_gamepad_axis_mvt(const GamepadID, const GamepadAxis) {
        return 0.f;
    }
    static bool is_gamepad_button_pressed(const GamepadID,
                                          const GamepadButton) {
        return false;
    }
    static bool is_gamepad_button_down(const GamepadID, const GamepadButton) {
        return false;
    }
    static void draw_text(const std::string &, const int, const int,
                          const int) {}
    static void set_gamepad_mappings(const std::string &) {}
    static std::string name_for_button(const GamepadButton) {
        return "unknown";
    }
    static std::string icon_for_button(const GamepadButton) {
        return "unknown";
    }
    static std::string icon_for_key(const int) { return "unknown"; }
#endif

    enum struct DeviceMedium {
        None,
        Keyboard,
        GamepadButton,
        GamepadAxis,
    };

    struct ActionDone {
        DeviceMedium medium;
        GamepadID id;
        int action;
        float amount_pressed;
        float length_pressed;

        ActionDone() = default;
        ActionDone(const ActionDone &) = default;
        ActionDone(ActionDone &&) = default;
        ActionDone &operator=(const ActionDone &) = default;
        ActionDone &operator=(ActionDone &&) = default;

        ActionDone(DeviceMedium m, GamepadID i, int a, float amount,
                   float length)
            : medium(m),
              id(i),
              action(a),
              amount_pressed(amount),
              length_pressed(length) {}
    };

    using ActionDoneInputAction = ActionDone;

    struct GamepadAxisWithDir {
        GamepadAxis axis;
        int dir = -1;
    };

    using AnyInput = std::variant<KeyCode, GamepadAxisWithDir, GamepadButton>;
    using ValidInputs = std::vector<AnyInput>;

    static float visit_key(const int keycode) {
        return is_key_pressed(keycode) ? 1.f : 0.f;
    }
    static float visit_key_down(const int keycode) {
        return is_key_down(keycode) ? 1.f : 0.f;
    }

    static float visit_axis(const GamepadID id,
                            const GamepadAxisWithDir axis_with_dir) {
        // Note: this one is a bit more complex because we have to check if you
        // are pushing in the right direction while also checking the magnitude
        const float mvt = get_gamepad_axis_mvt(id, axis_with_dir.axis);
        // Note: The 0.25 is how big the deadzone is
        // TODO consider making the deadzone configurable?
        if (util::sgn(mvt) == axis_with_dir.dir && abs(mvt) > DEADZONE) {
            return abs(mvt);
        }
        return 0.f;
    }

    static float visit_button(const GamepadID id, const GamepadButton button) {
        return is_gamepad_button_pressed(id, button) ? 1.f : 0.f;
    }

    static float visit_button_down(const GamepadID id,
                                   const GamepadButton button) {
        return is_gamepad_button_down(id, button) ? 1.f : 0.f;
    }

    struct InputCollector : public BaseComponent {
        std::vector<input::ActionDone> inputs;
        std::vector<input::ActionDone> inputs_pressed;
        float since_last_input = 0.f;
    };

    using InputCollectorInputAction = InputCollector;

    struct ProvidesMaxGamepadID : public BaseComponent {
        int max_gamepad_available = 0;
        size_t count() const { return (size_t) max_gamepad_available + 1; }
    };

    struct ProvidesInputMapping : public BaseComponent {
        using GameMapping = std::map<int, input::ValidInputs>;
        GameMapping mapping;
        ProvidesInputMapping(const GameMapping start_mapping)
            : mapping(start_mapping) {}
    };

    struct RenderConnectedGamepads : System<input::ProvidesMaxGamepadID> {
        virtual void for_each_with(
            const Entity &, const input::ProvidesMaxGamepadID &mxGamepadID,
            const float) const override {
            draw_text(("Gamepads connected: " +
                       std::to_string(mxGamepadID.max_gamepad_available)),
                      400, 60, 20);
        }
    };

    struct PossibleInputCollector {
        std::optional<std::reference_wrapper<InputCollector>> data;
        PossibleInputCollector(
            const std::optional<std::reference_wrapper<InputCollector>> d)
            : data(d) {}
        PossibleInputCollector(InputCollector &d) : data(d) {}
        PossibleInputCollector() : data({}) {}
        bool has_value() const { return data.has_value(); }
        bool valid() const { return has_value(); }

        [[nodiscard]] std::vector<input::ActionDone> &inputs() {
            return data.value().get().inputs;
        }

        [[nodiscard]] std::vector<input::ActionDone> &inputs_pressed() {
            return data.value().get().inputs_pressed;
        }

        [[nodiscard]] float since_last_input() {
            return data.value().get().since_last_input;
        }
    };

    static auto get_input_collector() -> PossibleInputCollector {
        // TODO replace with a singletone query
        OptEntity opt_collector =
            EntityQuery({.ignore_temp_warning = true})
                .template whereHasComponent<InputCollector>()
                .gen_first();
        if (!opt_collector.valid()) return {};
        Entity &collector = opt_collector.asE();
        return collector.get<InputCollector>();
    }

    // TODO i would like to move this out of input namespace
    // at some point
    struct InputSystem
        : System<InputCollector, ProvidesMaxGamepadID, ProvidesInputMapping> {
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

        std::pair<DeviceMedium, float> check_single_action_pressed(
            const GamepadID id, const input::ValidInputs valid_inputs) {
            DeviceMedium medium;
            float value = 0.f;
            for (const auto &input : valid_inputs) {
                DeviceMedium temp_medium = DeviceMedium::None;
                float temp = 0.f;
                // Use index-based access to avoid ambiguity with duplicate
                // types
                if (input.index() == 0) {
                    // KeyCode (int)
                    temp_medium = DeviceMedium::Keyboard;
                    temp = is_key_pressed(std::get<0>(input)) ? 1.f : 0.f;
                } else if (input.index() == 1) {
                    // GamepadAxisWithDir
                    temp_medium = DeviceMedium::GamepadAxis;
                    temp = visit_axis(id, std::get<1>(input));
                } else if (input.index() == 2) {
                    // GamepadButton
                    temp_medium = DeviceMedium::GamepadButton;
                    temp = is_gamepad_button_pressed(id, std::get<2>(input))
                               ? 1.f
                               : 0.f;
                }
                if (temp > value) {
                    value = temp;
                    medium = temp_medium;
                }
            }
            return {medium, value};
        }

        std::pair<DeviceMedium, float> check_single_action_down(
            const GamepadID id, const input::ValidInputs valid_inputs) {
            DeviceMedium medium;
            float value = 0.f;
            for (const auto &input : valid_inputs) {
                DeviceMedium temp_medium = DeviceMedium::None;
                float temp = 0.f;
                // Use index-based access to avoid ambiguity with duplicate
                // types
                if (input.index() == 0) {
                    // KeyCode (int)
                    temp_medium = DeviceMedium::Keyboard;
                    temp = visit_key_down(std::get<0>(input));
                } else if (input.index() == 1) {
                    // GamepadAxisWithDir
                    temp_medium = DeviceMedium::GamepadAxis;
                    temp = visit_axis(id, std::get<1>(input));
                } else if (input.index() == 2) {
                    // GamepadButton
                    temp_medium = DeviceMedium::GamepadButton;
                    temp = visit_button_down(id, std::get<2>(input));
                }
                if (temp > value) {
                    value = temp;
                    medium = temp_medium;
                }
            }
            return {medium, value};
        }

        virtual void for_each_with(Entity &, InputCollector &collector,
                                   ProvidesMaxGamepadID &mxGamepadID,
                                   ProvidesInputMapping &input_mapper,
                                   const float dt) override {
            mxGamepadID.max_gamepad_available =
                std::max(0, fetch_max_gampad_id());
            collector.inputs.clear();
            collector.inputs_pressed.clear();

            for (const auto &kv : input_mapper.mapping) {
                const int action = kv.first;
                const ValidInputs vis = kv.second;

                int i = 0;
                do {
                    // down
                    {
                        const auto [medium, amount] =
                            check_single_action_down(i, vis);
                        if (amount > 0.f) {
                            collector.inputs.push_back(
                                ActionDone(medium, i, action, amount, dt));
                        }
                    }
                    // pressed
                    {
                        const auto [medium, amount] =
                            check_single_action_pressed(i, vis);
                        if (amount > 0.f) {
                            collector.inputs_pressed.push_back(
                                ActionDone(medium, i, action, amount, dt));
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

    static void add_singleton_components(
        Entity &entity,
        const ProvidesInputMapping::GameMapping inital_mapping) {
        entity.addComponent<InputCollector>();
        entity.addComponent<input::ProvidesMaxGamepadID>();
        entity.addComponent<input::ProvidesInputMapping>(inital_mapping);

        EntityHelper::registerSingleton<InputCollector>(entity);
        EntityHelper::registerSingleton<input::ProvidesMaxGamepadID>(entity);
        EntityHelper::registerSingleton<input::ProvidesInputMapping>(entity);
    }

    static void enforce_singletons(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<developer::EnforceSingleton<InputCollector>>());
        sm.register_update_system(
            std::make_unique<
                developer::EnforceSingleton<ProvidesMaxGamepadID>>());
        sm.register_update_system(
            std::make_unique<
                developer::EnforceSingleton<ProvidesInputMapping>>());
    }

    static void register_update_systems(SystemManager &sm) {
        sm.register_update_system(
            std::make_unique<afterhours::input::InputSystem>());
    }

    // Renderer Systems:
    // RenderConnectedGamepads
};
}  // namespace afterhours
