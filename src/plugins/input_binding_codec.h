#pragma once

#include "input_system.h"
#include <array>
#include <charconv>
#include <optional>
#include <span>
#include <string_view>

namespace afterhours::input_binding_codec {

namespace detail {
// Persisted enums save by name so reordering values never breaks old saves.
// Key codes stay numeric: they are external codes, not reordered enums.
struct NamedCode {
  int value;
  std::string_view name;
};

constexpr NamedCode GAMEPAD_BUTTONS[] = {
    {gamepad_buttons::LEFT_FACE_UP, "LeftFaceUp"},
    {gamepad_buttons::LEFT_FACE_RIGHT, "LeftFaceRight"},
    {gamepad_buttons::LEFT_FACE_DOWN, "LeftFaceDown"},
    {gamepad_buttons::LEFT_FACE_LEFT, "LeftFaceLeft"},
    {gamepad_buttons::RIGHT_FACE_UP, "RightFaceUp"},
    {gamepad_buttons::RIGHT_FACE_RIGHT, "RightFaceRight"},
    {gamepad_buttons::RIGHT_FACE_DOWN, "RightFaceDown"},
    {gamepad_buttons::RIGHT_FACE_LEFT, "RightFaceLeft"},
    {gamepad_buttons::LEFT_TRIGGER_1, "LeftTrigger1"},
    {gamepad_buttons::LEFT_TRIGGER_2, "LeftTrigger2"},
    {gamepad_buttons::RIGHT_TRIGGER_1, "RightTrigger1"},
    {gamepad_buttons::RIGHT_TRIGGER_2, "RightTrigger2"},
    {gamepad_buttons::MIDDLE_LEFT, "MiddleLeft"},
    {gamepad_buttons::MIDDLE, "Middle"},
    {gamepad_buttons::MIDDLE_RIGHT, "MiddleRight"},
    {gamepad_buttons::LEFT_THUMB, "LeftThumb"},
    {gamepad_buttons::RIGHT_THUMB, "RightThumb"},
};

constexpr NamedCode GAMEPAD_AXES[] = {
    {gamepad_axes::LEFT_X, "LeftX"},       {gamepad_axes::LEFT_Y, "LeftY"},
    {gamepad_axes::RIGHT_X, "RightX"},     {gamepad_axes::RIGHT_Y, "RightY"},
    {gamepad_axes::LEFT_TRIGGER, "LeftTrigger"},
    {gamepad_axes::RIGHT_TRIGGER, "RightTrigger"},
};

constexpr NamedCode MOUSE_AXES[] = {
    {static_cast<int>(input::MouseAxis::X), "X"},
    {static_cast<int>(input::MouseAxis::Y), "Y"},
};

inline std::optional<int> integer(std::string_view text) {
    int value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        return std::nullopt;
    return value;
}

// Encode: the code's name, or its int when it has none (e.g. a fallback
// GamepadButton built from an arbitrary int).
template <size_t N>
inline std::string code_name(const NamedCode (&table)[N], int code) {
    for (const auto &entry : table)
        if (entry.value == code) return std::string(entry.name);
    return std::to_string(code);
}

// Decode accepts the name (new saves) or the legacy int (old saves).
template <size_t N>
inline std::optional<int> code_value(const NamedCode (&table)[N],
                                     std::string_view text, int lo, int hi) {
    for (const auto &entry : table)
        if (entry.name == text) return entry.value;
    const auto code = integer(text);
    if (!code || *code < lo || *code > hi) return std::nullopt;
    return code;
}

inline bool valid_key(int key) {
    if ((key >= keys::ZERO && key <= keys::NINE) ||
        (key >= keys::A && key <= keys::Z) ||
        (key >= keys::ESCAPE && key <= keys::END) ||
        (key >= keys::CAPS_LOCK && key <= keys::PAUSE) ||
        (key >= keys::F1 && key <= keys::F12) ||
        (key >= keys::KP_0 && key <= keys::KP_EQUAL) ||
        (key >= keys::LEFT_SHIFT && key <= keys::KB_MENU)) return true;
    for (int symbol : {keys::SPACE, keys::APOSTROPHE, keys::COMMA, keys::MINUS,
                       keys::PERIOD, keys::SLASH, keys::SEMICOLON, keys::EQUAL,
                       keys::LEFT_BRACKET, keys::BACKSLASH, keys::RIGHT_BRACKET,
                       keys::GRAVE})
        if (key == symbol) return true;
    return false;
}
}

inline std::string encode(const input::AnyInput &binding) {
    return std::visit([](const auto &value) -> std::string {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, input::KeyChord>) {
            return "v1:key:" + std::to_string(value.key) + ":" +
                   std::to_string(value.required_modifiers) + ":" +
                   std::to_string(value.has_explicit_modifiers);
        } else if constexpr (std::is_same_v<T, input::GamepadAxisWithDir>) {
            return "v1:gamepad_axis:" +
                   detail::code_name(detail::GAMEPAD_AXES,
                                     static_cast<int>(value.axis)) +
                   ":" + std::to_string(value.dir);
        } else if constexpr (std::is_same_v<T, input::MouseAxisWithDir>) {
            return "v1:mouse_axis:" +
                   detail::code_name(detail::MOUSE_AXES,
                                     static_cast<int>(value.axis)) +
                   ":" + std::to_string(value.dir);
        } else if constexpr (std::is_enum_v<T>) {
            return "v1:gamepad_button:" +
                   detail::code_name(detail::GAMEPAD_BUTTONS,
                                     static_cast<int>(value));
        } else {
            return "v1:gamepad_button:" +
                   detail::code_name(detail::GAMEPAD_BUTTONS, value.value);
        }
    }, binding);
}

inline std::optional<input::AnyInput> decode(std::string_view text) {
    std::array<std::string_view, 5> fields;
    std::size_t count = 0;
    for (;;) {
        if (count == fields.size()) return std::nullopt;
        const auto split = text.find(':');
        fields[count++] = text.substr(0, split);
        if (split == std::string_view::npos) break;
        text.remove_prefix(split + 1);
    }
    if (count < 3 || fields[0] != "v1") return std::nullopt;
    if (fields[1] == "key") {
        const auto code = detail::integer(fields[2]);
        if (!code) return std::nullopt;
        if (count != 5 || !detail::valid_key(*code)) return std::nullopt;
        const auto modifiers = detail::integer(fields[3]);
        const auto explicit_modifiers = detail::integer(fields[4]);
        if (!modifiers || *modifiers < 0 || *modifiers > 15 ||
            !explicit_modifiers || (*explicit_modifiers != 0 && *explicit_modifiers != 1))
            return std::nullopt;
        input::KeyChord key{*code};
        key.required_modifiers = static_cast<std::uint8_t>(*modifiers);
        key.has_explicit_modifiers = *explicit_modifiers != 0;
        return key;
    }
    if (fields[1] == "gamepad_button") {
        if (count != 3) return std::nullopt;
        const auto code = detail::code_value(detail::GAMEPAD_BUTTONS, fields[2],
                                             gamepad_buttons::LEFT_FACE_UP,
                                             gamepad_buttons::RIGHT_THUMB);
        if (!code) return std::nullopt;
        return static_cast<input::GamepadButton>(*code);
    }
    if (count != 4) return std::nullopt;
    const auto direction = detail::integer(fields[3]);
    if (!direction || (*direction != -1 && *direction != 1)) return std::nullopt;
    if (fields[1] == "gamepad_axis") {
        const auto code = detail::code_value(detail::GAMEPAD_AXES, fields[2],
                                             gamepad_axes::LEFT_X,
                                             gamepad_axes::RIGHT_TRIGGER);
        if (!code) return std::nullopt;
        return input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(*code), *direction};
    }
    if (fields[1] == "mouse_axis") {
        const auto code = detail::code_value(detail::MOUSE_AXES, fields[2], 0, 1);
        if (!code) return std::nullopt;
        return input::MouseAxisWithDir{static_cast<input::MouseAxis>(*code), *direction};
    }
    return std::nullopt;
}

inline std::vector<std::string> encode_bindings(const input::ValidInputs &bindings) {
    std::vector<std::string> result;
    result.reserve(bindings.size());
    for (const auto &binding : bindings) result.push_back(encode(binding));
    return result;
}

inline std::optional<input::ValidInputs> decode_bindings(std::span<const std::string> records) {
    input::ValidInputs result;
    result.reserve(records.size());
    for (const auto &record : records) {
        auto binding = decode(record);
        if (!binding) return std::nullopt;
        result.push_back(std::move(*binding));
    }
    return result;
}

}
