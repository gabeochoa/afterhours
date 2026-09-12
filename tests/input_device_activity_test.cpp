#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_prompts.h>
#include <cassert>

namespace {
bool key_pressed = false, button_pressed = false, mouse_pressed = false;
float axis_value = 0;
}
namespace raylib {
Vector2 GetMouseDelta() { return {}; }
bool IsGamepadAvailable(int id) { return id == 0; }
bool IsKeyPressed(int key) { return key == afterhours::keys::A && key_pressed; }
bool IsKeyPressedRepeat(int) { return false; }
bool IsKeyDown(int key) { return key == afterhours::keys::A && key_pressed; }
bool IsMouseButtonDown(int button) { return button == 0 && mouse_pressed; }
bool IsGamepadButtonPressed(int, int) { return button_pressed; }
bool IsGamepadButtonDown(int, int) { return button_pressed; }
float GetGamepadAxisMovement(int, int axis) { return axis == 0 ? axis_value : 0; }
}

int main() {
    using namespace afterhours;
    input::InputCollector collector;
    input::ProvidesMaxGamepadID gamepads;
    input::ProvidesInputMapping mapper({{1, {input::KeyChord{keys::A},
        input::GamepadAxisWithDir{static_cast<input::GamepadAxis>(0), 1},
        static_cast<input::GamepadButton>(7)}}});
    input::InputSystem normal;
    Entity entity;
    enum class Layer { Main, Empty };
    ProvidesLayeredInputMapping<Layer> layered;
    layered.set_binding(Layer::Main, 1, mapper.mapping[1]);
    layered.set_active_layer(Layer::Main);
    LayeredInputSystem<Layer> layered_system;
    for (bool use_layers : {false, true}) {
        auto frame = [&] {
            if (use_layers) layered_system.for_each_with(entity, collector, gamepads, layered, .016f);
            else normal.for_each_with(entity, collector, gamepads, mapper, .016f);
        };
        axis_value = .1f;
        frame();
        assert(collector.device_activity.preferred() == input::Device::KeyboardMouse);
        axis_value = .8f;
        frame();
        assert(collector.device_activity.preferred() == input::Device::Gamepad);
        key_pressed = true;
        frame();
        assert(collector.inputs_pressed[0].medium == input::DeviceMedium::Keyboard);
        assert(collector.device_activity.preferred() == input::Device::KeyboardMouse);
        key_pressed = false;
        frame();
        assert(collector.device_activity.preferred() == input::Device::KeyboardMouse);
        axis_value = 0;
        frame();
        axis_value = .8f;
        frame();
        assert(collector.device_activity.preferred() == input::Device::Gamepad);
        mouse_pressed = true;
        frame();
        assert(collector.device_activity.preferred() == input::Device::KeyboardMouse);
        mouse_pressed = false;
        frame();
        assert(collector.device_activity.preferred() == input::Device::KeyboardMouse);
    }
}
