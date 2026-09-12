#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_prompts.h>
#include <cassert>

int main() {
    using namespace afterhours;
    using namespace input_prompts;
    assert(format_binding(input::KeyChord{keys::A, 15}) == "Ctrl+Shift+Alt+Super+A");
    assert(format_binding(input::MouseAxisWithDir{input::MouseAxis::Y, -1}) == "Mouse up");
    assert(format_binding(input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(0), 1}) == "Left stick X +");
    input::InputCollector collector;
    DevicePreference device;
    auto observe = [&](input::BindingActivity activity, bool mouse = false) {
        collector.device_activity.begin_frame();
        collector.device_activity.observe(0, activity);
        collector.device_activity.end_frame(mouse);
    };
    observe({});
    assert(device.preferred(collector) == Device::KeyboardMouse);
    observe({false, false, 1});
    assert(device.preferred(collector) == Device::Gamepad);
    observe({true, false, 1});
    assert(device.preferred(collector) == Device::KeyboardMouse);
    observe({false, false, 1});
    assert(device.preferred(collector) == Device::KeyboardMouse);
    observe({});
    observe({false, false, 1});
    assert(device.preferred(collector) == Device::Gamepad);
    device.pinned = Device::KeyboardMouse;
    observe({false, true, 0});
    assert(device.preferred(collector) == Device::KeyboardMouse);
    device.pinned.reset();
    assert(device.preferred(collector) == Device::Gamepad);
    observe({}, true);
    assert(device.preferred(collector) == Device::KeyboardMouse);
    input::ValidInputs alternatives{input::KeyChord{keys::A}, static_cast<input::GamepadButton>(7)};
    assert(format_binding(input::KeyChord{keys::ENTER}, [](int) { return "Return"; }) == "Return");
    const BindingFormatter translated = [](const input::AnyInput &) { return "Entrée"; };
    assert(prompt_for(alternatives, Device::KeyboardMouse, translated)->label == "Entrée");
    enum class Layer { Menu, Game };
    ProvidesLayeredInputMapping<Layer> mapping;
    mapping.set_binding(Layer::Menu, 1, {input::KeyChord{keys::ENTER}});
    mapping.set_active_layer(Layer::Menu);
    device.pinned = Device::KeyboardMouse;
    assert(prompt_for(mapping, 1, collector, device)->label == "Enter");
    mapping.set_binding(Layer::Menu, 1, {input::KeyChord{keys::SPACE}});
    assert(prompt_for(mapping, 1, collector, device)->label == "Space");
    mapping.set_active_layer(Layer::Game);
    assert(!prompt_for(mapping, 1, collector, device));
}
