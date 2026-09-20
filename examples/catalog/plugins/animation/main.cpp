#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../../../src/plugins/animation.h"

using namespace afterhours;
using motion::Spring;
using motion::Timeline;

enum struct AnimKey {
    FadeIn,
    Position,
    Scale,
};

int main() {
    std::cout << "=== Animation Plugin Example ===" << std::endl;

    SystemManager systems;
    animation::register_update_systems(systems);
    auto step = [&](float seconds, float dt = 1.f / 60.f) {
        for (float t = 0.f; t < seconds - 1e-6f; t += dt) systems.run(dt);
    };

    std::cout << "\n1. Timed fade-in (0.0 -> 1.0 over 0.5s, linear):" << std::endl;
    motion::anim(AnimKey::FadeIn).from(0.f).to(1.f, Timeline{.keys = {{0.f, 0.f}, {0.5f, 1.f}}});
    assert(motion::anim(AnimKey::FadeIn).active());
    step(0.25f);
    float val = motion::anim(AnimKey::FadeIn).value();
    std::cout << "  - After 0.25s: value = " << val << " (expected ~0.5)" << std::endl;
    assert(val > 0.4f && val < 0.6f);
    step(0.3f);
    assert(!motion::anim(AnimKey::FadeIn).active());
    std::cout << "  - After 0.55s: settled at " << motion::anim(AnimKey::FadeIn).value() << std::endl;

    std::cout << "\n2. Spring (0 -> 100, snappy):" << std::endl;
    motion::anim(AnimKey::Position).from(0.f).to(100.f, Spring::snappy());
    step(0.1f);
    val = motion::anim(AnimKey::Position).value();
    std::cout << "  - At 0.1s: " << val << " (should be well past 50)" << std::endl;
    assert(val > 50.f);
    step(1.f);
    val = motion::anim(AnimKey::Position).value();
    std::cout << "  - Settled: " << val << std::endl;
    assert(val > 99.9f && val < 100.1f);

    std::cout << "\n3. Chained steps (1.0 -> 2.0 -> 0.5 -> 1.0):" << std::endl;
    const Timeline half_second{.keys = {{0.f, 0.f}, {0.5f, 1.f}}};
    motion::anim(AnimKey::Scale).from(1.f).to(2.f, half_second).then(0.5f, half_second).then(1.f, half_second);
    step(0.5f);
    val = motion::anim(AnimKey::Scale).value();
    std::cout << "  - After segment 1: " << val << " (expected 2.0)" << std::endl;
    assert(val > 1.9f && val < 2.1f);
    step(0.5f);
    val = motion::anim(AnimKey::Scale).value();
    std::cout << "  - After segment 2: " << val << " (expected 0.5)" << std::endl;
    assert(val > 0.4f && val < 0.6f);
    step(0.6f);
    val = motion::anim(AnimKey::Scale).value();
    std::cout << "  - After segment 3: " << val << " (expected 1.0)" << std::endl;
    assert(val > 0.9f && val < 1.1f);

    std::cout << "\n4. Completion callback:" << std::endl;
    bool callback_fired = false;
    motion::anim(AnimKey::FadeIn)
        .from(0.f)
        .to(1.f, Timeline{.keys = {{0.f, 0.f}, {0.1f, 1.f}}})
        .on_complete([&callback_fired]() {
            callback_fired = true;
            std::cout << "  - on_complete callback fired!" << std::endl;
        });
    step(0.2f);
    assert(callback_fired);

    std::cout << "\n5. Instant mode lands on the last step immediately:" << std::endl;
    animation::set_instant(true);
    motion::anim(AnimKey::Scale).from(1.f).to(2.f, half_second).then(3.f, half_second);
    systems.run(1.f / 60.f);
    val = motion::anim(AnimKey::Scale).value();
    std::cout << "  - After one frame: " << val << " (expected 3.0)" << std::endl;
    assert(val == 3.f);
    animation::set_instant(false);

    std::cout << "\n=== All animation tests passed! ===" << std::endl;
    return 0;
}
