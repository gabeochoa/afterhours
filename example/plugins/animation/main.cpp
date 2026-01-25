#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/animation.h"

using namespace afterhours;

// Define animation keys as an enum
enum struct AnimKey {
    FadeIn,
    Position,
    Scale,
};

int main() {
    std::cout << "=== Animation Plugin Example ===" << std::endl;

    // Register animation update system
    SystemManager systems;
    animation::register_update_systems<AnimKey>(systems);

    // Test 1: Simple fade-in animation
    std::cout << "\n1. Simple fade-in animation:" << std::endl;
    animation::anim<AnimKey>(AnimKey::FadeIn)
        .from(0.0f)
        .to(1.0f, 0.5f, animation::EasingType::Linear);

    assert(animation::manager<AnimKey>().is_active(AnimKey::FadeIn));
    std::cout << "  - Fade animation started (0.0 -> 1.0 over 0.5s)" << std::endl;

    // Simulate time passing
    float dt = 0.25f;
    animation::manager<AnimKey>().update(dt);
    auto val = animation::manager<AnimKey>().get_value(AnimKey::FadeIn);
    assert(val.has_value());
    std::cout << "  - After 0.25s: value = " << val.value() << " (expected ~0.5)" << std::endl;
    assert(val.value() > 0.4f && val.value() < 0.6f);

    animation::manager<AnimKey>().update(dt);
    val = animation::manager<AnimKey>().get_value(AnimKey::FadeIn);
    if (val.has_value()) {
        std::cout << "  - After 0.50s: value = " << val.value() << " (expected 1.0)" << std::endl;
    } else {
        std::cout << "  - After 0.50s: animation completed (track inactive)" << std::endl;
    }
    assert(!animation::manager<AnimKey>().is_active(AnimKey::FadeIn));

    // Test 2: Easing functions
    std::cout << "\n2. Easing functions:" << std::endl;
    animation::anim<AnimKey>(AnimKey::Position)
        .from(0.0f)
        .to(100.0f, 1.0f, animation::EasingType::EaseOutQuad);

    // Check at 50% time - EaseOutQuad should be more than 50% there
    animation::manager<AnimKey>().update(0.5f);
    val = animation::manager<AnimKey>().get_value(AnimKey::Position);
    std::cout << "  - EaseOutQuad at 50% time: " << val.value() << " (should be > 50)" << std::endl;
    assert(val.value() > 50.0f); // EaseOutQuad is faster at start

    animation::manager<AnimKey>().update(0.5f);
    val = animation::manager<AnimKey>().get_value(AnimKey::Position);
    if (val.has_value()) {
        std::cout << "  - EaseOutQuad at 100% time: " << val.value() << std::endl;
    } else {
        std::cout << "  - EaseOutQuad at 100% time: animation completed" << std::endl;
    }

    // Test 3: Animation sequence
    std::cout << "\n3. Animation sequence (chained animations):" << std::endl;
    animation::anim<AnimKey>(AnimKey::Scale)
        .from(1.0f)
        .sequence({
            {.to_value = 2.0f, .duration = 0.5f, .easing = animation::EasingType::Linear},
            {.to_value = 0.5f, .duration = 0.5f, .easing = animation::EasingType::Linear},
            {.to_value = 1.0f, .duration = 0.5f, .easing = animation::EasingType::Linear},
        });

    std::cout << "  - Sequence: 1.0 -> 2.0 -> 0.5 -> 1.0" << std::endl;

    animation::manager<AnimKey>().update(0.5f);
    val = animation::manager<AnimKey>().get_value(AnimKey::Scale);
    std::cout << "  - After segment 1: " << val.value() << " (expected 2.0)" << std::endl;
    assert(val.value() > 1.9f && val.value() < 2.1f);

    animation::manager<AnimKey>().update(0.5f);
    val = animation::manager<AnimKey>().get_value(AnimKey::Scale);
    std::cout << "  - After segment 2: " << val.value() << " (expected 0.5)" << std::endl;
    assert(val.value() > 0.4f && val.value() < 0.6f);

    animation::manager<AnimKey>().update(0.5f);
    val = animation::manager<AnimKey>().get_value(AnimKey::Scale);
    if (val.has_value()) {
        std::cout << "  - After segment 3: " << val.value() << " (expected 1.0)" << std::endl;
        assert(val.value() > 0.9f && val.value() < 1.1f);
    } else {
        std::cout << "  - After segment 3: animation completed (track inactive)" << std::endl;
    }

    // Test 4: on_complete callback
    std::cout << "\n4. Animation completion callback:" << std::endl;
    bool callback_fired = false;
    animation::anim<AnimKey>(AnimKey::FadeIn)
        .from(0.0f)
        .to(1.0f, 0.1f, animation::EasingType::Linear)
        .on_complete([&callback_fired]() {
            callback_fired = true;
            std::cout << "  - on_complete callback fired!" << std::endl;
        });

    animation::manager<AnimKey>().update(0.1f);
    assert(callback_fired);

    // Test 5: one_shot helper
    std::cout << "\n5. one_shot animation (only starts once):" << std::endl;
    int setup_count = 0;
    for (int i = 0; i < 3; ++i) {
        animation::one_shot<AnimKey>(AnimKey::Position, [&setup_count](auto anim) {
            setup_count++;
            anim.from(0.0f).to(100.0f, 1.0f, animation::EasingType::Linear);
        });
    }
    std::cout << "  - Called one_shot 3 times, setup ran " << setup_count << " time(s)" << std::endl;
    assert(setup_count == 1);

    std::cout << "\n=== All animation tests passed! ===" << std::endl;
    return 0;
}
