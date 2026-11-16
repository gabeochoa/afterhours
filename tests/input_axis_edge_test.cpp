#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include <cassert>

namespace {
float trigger = 0;
constexpr int TRIGGER_AXIS = 5;
}
namespace raylib {
Vector2 GetMouseDelta() { return {}; }
bool IsGamepadAvailable(int id) { return id == 0; }
bool IsKeyPressed(int) { return false; }
bool IsKeyPressedRepeat(int) { return false; }
bool IsKeyDown(int) { return false; }
bool IsMouseButtonDown(int) { return false; }
bool IsGamepadButtonPressed(int, int) { return false; }
bool IsGamepadButtonDown(int, int) { return false; }
float GetGamepadAxisMovement(int, int axis) { return axis == TRIGGER_AXIS ? trigger : 0; }
}

int main() {
    using namespace afterhours;
    input::InputCollector collector;
    input::ProvidesMaxGamepadID gamepads;
    input::ProvidesInputMapping mapper({{1, {input::GamepadAxisWithDir{
        static_cast<input::GamepadAxis>(TRIGGER_AXIS), 1}}}});
    input::InputSystem normal;
    Entity entity;
    enum class Layer { Main };
    ProvidesLayeredInputMapping<Layer> layered;
    layered.set_binding(Layer::Main, 1, mapper.mapping[1]);
    layered.set_active_layer(Layer::Main);
    LayeredInputSystem<Layer> layered_system;
    for (bool use_layers : {false, true}) {
        auto frame = [&](float value) {
            trigger = value;
            if (use_layers) layered_system.for_each_with(entity, collector, gamepads, layered, .016f);
            else normal.for_each_with(entity, collector, gamepads, mapper, .016f);
        };
        frame(0);
        frame(.2f);
        assert(collector.inputs.empty() && collector.inputs_pressed.empty());
        frame(.9f);
        assert(collector.inputs.size() == 1 && collector.inputs_pressed.size() == 1);
        assert(collector.inputs_pressed_repeat.size() == 1);
        for (int i = 0; i < 3; ++i) {
            frame(.9f);
            assert(collector.inputs.size() == 1);
            assert(collector.inputs_pressed.empty() && collector.inputs_pressed_repeat.empty());
        }
        frame(.1f);
        assert(collector.inputs.empty() && collector.inputs_pressed.empty());
        frame(.9f);
        assert(collector.inputs_pressed.size() == 1);
        frame(0);
    }
}
