#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <afterhours/src/plugins/input_binding_codec.h>
#include <cassert>

int main() {
    using namespace afterhours;
    using namespace input_binding_codec;
    const input::ValidInputs originals{
        input::KeyChord{keys::A}, input::KeyChord{keys::A, 0},
        input::KeyChord{keys::ENTER, 15},
        input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(0), -1},
        input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(5), 1},
        static_cast<input::GamepadButton>(7),
        input::MouseAxisWithDir{input::MouseAxis::X, -1},
        input::MouseAxisWithDir{input::MouseAxis::Y, 1}};
    // Enums save by name; key codes stay numeric (external codes, not enums).
    const std::vector<std::string> expected{
        "v1:key:65:0:0", "v1:key:65:0:1", "v1:key:257:15:1",
        "v1:gamepad_axis:LeftX:-1", "v1:gamepad_axis:RightTrigger:1",
        "v1:gamepad_button:RightFaceDown", "v1:mouse_axis:X:-1",
        "v1:mouse_axis:Y:1"};
    assert(encode_bindings(originals) == expected);
    auto decoded = decode_bindings(expected);
    assert(decoded && decoded->size() == originals.size());
    assert(!std::get<input::KeyChord>((*decoded)[0]).has_explicit_modifiers);
    assert(std::get<input::KeyChord>((*decoded)[1]).has_explicit_modifiers);
    const auto chord = std::get<input::KeyChord>((*decoded)[2]);
    assert(chord.key == keys::ENTER && chord.required_modifiers == 15);
    assert(std::get<input::GamepadAxisWithDir>((*decoded)[3]).dir == -1);
    assert(std::get<input::MouseAxisWithDir>((*decoded)[7]).axis == input::MouseAxis::Y);
    assert(encode_bindings(*decoded) == expected);
    assert(decode_bindings({})->empty());
    // Old saves wrote ints; they still load (and re-save by name).
    const std::vector<std::string> legacy{
        "v1:gamepad_axis:0:-1", "v1:gamepad_axis:5:1", "v1:gamepad_button:7",
        "v1:mouse_axis:0:-1", "v1:mouse_axis:1:1"};
    auto old = decode_bindings(legacy);
    assert(old && old->size() == legacy.size());
    assert(std::get<input::GamepadAxisWithDir>((*old)[0]).axis ==
           static_cast<input::GamepadAxis>(0));
    assert(std::get<input::GamepadAxisWithDir>((*old)[1]).axis ==
           static_cast<input::GamepadAxis>(5));
    assert(std::get<input::GamepadButton>((*old)[2]) ==
           static_cast<input::GamepadButton>(7));
    assert(std::get<input::MouseAxisWithDir>((*old)[3]).axis == input::MouseAxis::X);
    const std::vector<std::string> legacy_named{
        "v1:gamepad_axis:LeftX:-1", "v1:gamepad_axis:RightTrigger:1",
        "v1:gamepad_button:RightFaceDown", "v1:mouse_axis:X:-1",
        "v1:mouse_axis:Y:1"};
    assert(*old == *decode_bindings(legacy_named));
    assert(encode_bindings(*old) == legacy_named);
    for (auto invalid : {"", "v2:key:65:0:0", "v1:key:0:0:0", "v1:key:65:16:1",
                         "v1:key:65:1:2", "v1:gamepad_button:18", "v1:mouse_axis:2:1",
                         "v1:gamepad_axis:6:1", "v1:gamepad_axis:0:0", "v1:key:65:0:0:x",
                         "v1:unknown:1", "v1:key:999999999999:0:0", "v1:key:65x:0:0",
                         "v1:gamepad_button:NoSuchButton", "v1:gamepad_axis:NoAxis:1",
                         "v1:mouse_axis:Z:1", "v1:gamepad_button:", "v1:gamepad_button:0"})
        assert(!decode(invalid));
    auto broken = expected;
    broken.back() = "invalid";
    assert(!decode_bindings(broken));
}
