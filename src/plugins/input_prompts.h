#pragma once

#include "input_system.h"
#include <optional>
#include <functional>

namespace afterhours::input_prompts {

using Device = input::Device;

struct DevicePreference {
    std::optional<Device> pinned;
    Device preferred(const input::InputCollector &collector) const {
        return pinned.value_or(collector.device_activity.preferred());
    }
};

using KeyName = std::function<std::string(input::KeyCode)>;
using BindingFormatter = std::function<std::string(const input::AnyInput &)>;

inline std::string key_label(int key) {
    if (key > keys::SPACE && key <= keys::GRAVE) return std::string(1, static_cast<char>(key));
    if (key >= keys::F1 && key <= keys::F12) return "F" + std::to_string(key - keys::F1 + 1);
    if (key >= keys::KP_0 && key <= keys::KP_9) return "Keypad " + std::to_string(key - keys::KP_0);
    switch (key) {
        case keys::SPACE: return "Space";
        case keys::ENTER: return "Enter";
        case keys::ESCAPE: return "Esc";
        case keys::TAB: return "Tab";
        case keys::BACKSPACE: return "Backspace";
        case keys::DELETE_KEY: return "Delete";
        case keys::INSERT: return "Insert";
        case keys::HOME: return "Home";
        case keys::END: return "End";
        case keys::PAGE_UP: return "Page Up";
        case keys::PAGE_DOWN: return "Page Down";
        case keys::UP: return "Up";
        case keys::DOWN: return "Down";
        case keys::LEFT: return "Left";
        case keys::RIGHT: return "Right";
        case keys::LEFT_SHIFT: return "Left Shift";
        case keys::RIGHT_SHIFT: return "Right Shift";
        case keys::LEFT_CONTROL: return "Left Ctrl";
        case keys::RIGHT_CONTROL: return "Right Ctrl";
        case keys::LEFT_ALT: return "Left Alt";
        case keys::RIGHT_ALT: return "Right Alt";
        case keys::LEFT_SUPER: return "Left Super";
        case keys::RIGHT_SUPER: return "Right Super";
        case keys::CAPS_LOCK: return "Caps Lock";
        case keys::SCROLL_LOCK: return "Scroll Lock";
        case keys::NUM_LOCK: return "Num Lock";
        case keys::PRINT_SCREEN: return "Print Screen";
        case keys::PAUSE: return "Pause";
        case keys::KP_DECIMAL: return "Keypad .";
        case keys::KP_DIVIDE: return "Keypad /";
        case keys::KP_MULTIPLY: return "Keypad *";
        case keys::KP_SUBTRACT: return "Keypad -";
        case keys::KP_ADD: return "Keypad +";
        case keys::KP_ENTER: return "Keypad Enter";
        case keys::KP_EQUAL: return "Keypad =";
        case keys::KB_MENU: return "Menu";
        default: return "Key " + std::to_string(key);
    }
}

inline Device device_for(const input::AnyInput &binding) {
    if (std::holds_alternative<input::KeyChord>(binding) ||
        std::holds_alternative<input::MouseAxisWithDir>(binding)) return Device::KeyboardMouse;
    return Device::Gamepad;
}

inline std::string format_binding(const input::AnyInput &binding, const KeyName &key_name = key_label) {
    return std::visit([&key_name](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, input::KeyChord>) {
            std::string label;
            if (value.required_modifiers & input::KeyChord::MOD_CTRL) label += "Ctrl+";
            if (value.required_modifiers & input::KeyChord::MOD_SHIFT) label += "Shift+";
            if (value.required_modifiers & input::KeyChord::MOD_ALT) label += "Alt+";
            if (value.required_modifiers & input::KeyChord::MOD_SUPER) label += "Super+";
            return label + key_name(value.key);
        } else if constexpr (std::is_same_v<T, input::MouseAxisWithDir>) {
            if (value.axis == input::MouseAxis::X) return value.dir < 0 ? "Mouse left" : "Mouse right";
            return value.dir < 0 ? "Mouse up" : "Mouse down";
        } else if constexpr (std::is_same_v<T, input::GamepadAxisWithDir>) {
            static constexpr const char *names[] = {"Left stick X", "Left stick Y", "Right stick X", "Right stick Y", "Left trigger", "Right trigger"};
            const int axis = static_cast<int>(value.axis);
            const auto name = axis >= 0 && axis < 6 ? std::string(names[axis]) : "Axis " + std::to_string(axis);
            return name + (value.dir < 0 ? " -" : " +");
        } else {
            int button;
            if constexpr (std::is_enum_v<T>) button = static_cast<int>(value);
            else button = value.value;
            static constexpr const char *names[] = {"Unknown", "D-pad up", "D-pad right", "D-pad down", "D-pad left", "Face up", "Face right", "Face down", "Face left", "Left shoulder", "Left trigger", "Right shoulder", "Right trigger", "Select", "Guide", "Start", "Left stick", "Right stick"};
            if (button > 0 && button < 18) return names[button];
            return "Button " + std::to_string(button);
        }
    }, binding);
}

struct Prompt {
    input::AnyInput binding;
    std::string label;
};

inline std::optional<Prompt> prompt_for(const input::ValidInputs &bindings, Device device,
                                        const BindingFormatter &formatter = [](const input::AnyInput &binding) { return format_binding(binding); }) {
    for (const auto &binding : bindings) {
        if (device_for(binding) != device) continue;
        return Prompt{binding, formatter(binding)};
    }
    return std::nullopt;
}

template<typename Layer>
std::optional<Prompt> prompt_for(const ProvidesLayeredInputMapping<Layer> &mapping,
                                int action, const input::InputCollector &collector,
                                const DevicePreference &preference = {},
                                const BindingFormatter &formatter = [](const input::AnyInput &binding) { return format_binding(binding); }) {
    return prompt_for(mapping.get_bindings(action), preference.preferred(collector), formatter);
}

}
