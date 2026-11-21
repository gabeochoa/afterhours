#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#include <cassert>

using namespace afterhours;
namespace inj = testing::input_injector;

int main() {
    testing::test_input::detail::test_mode = true;
    inj::reset_all();

    Entity &entity = EntityHelper::createPermanentEntity();
    input::add_singleton_components(
        entity, input::ProvidesInputMapping::GameMapping{
                    {1, {input::KeyChord{keys::A}}}});
    auto &collector = entity.get<input::InputCollector>();
    input::InputSystem system;
    auto frame = [&] {
        system.for_each_with(entity, collector,
                             entity.get<input::ProvidesMaxGamepadID>(),
                             entity.get<input::ProvidesInputMapping>(), .016f);
    };
    auto snapshot = [&] { return input::input_activity(); };

    frame();
    auto idle = snapshot();
    assert(!idle.any && !idle.pressed && idle.frame == 1 &&
           idle.last_activity_frame == 0);

    testing::test_input::push_key(keys::A);
    inj::reset_frame();
    frame();
    auto press = snapshot();
    assert(press.any && press.pressed && press.frame == 2 &&
           press.last_activity_frame == 2);
    assert(snapshot().last_activity_frame == 2);
    assert(collector.inputs_pressed.size() == 1 &&
           collector.inputs_pressed[0].action == 1);

    inj::reset_frame();
    inj::set_key_held(keys::A);
    frame();
    auto held = snapshot();
    assert(held.any && !held.pressed && held.last_activity_frame == 3);

    inj::set_key_up(keys::A);
    inj::reset_frame();
    frame();
    auto idle2 = snapshot();
    assert(!idle2.any && !idle2.pressed && idle2.frame == 4 &&
           idle2.last_activity_frame == 3);

    input::InputCollector axis_collector;
    axis_collector.inputs.emplace_back(input::DeviceMedium::GamepadAxis, 0, 1,
                                       .8f, .016f);
    axis_collector.note_activity_frame();
    assert(axis_collector.activity.any && !axis_collector.activity.pressed &&
           axis_collector.activity.last_activity_frame == 1);
    axis_collector.inputs.clear();
    axis_collector.note_activity_frame();
    assert(!axis_collector.activity.any &&
           axis_collector.activity.last_activity_frame == 1);

    inj::reset_all();
    testing::test_input::detail::test_mode = false;
    return 0;
}
