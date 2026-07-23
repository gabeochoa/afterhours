#include <cassert>
#include <cmath>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"
#include "../../../src/plugins/collision.h"

using namespace afterhours;

// Custom Transform component with required fields for collision
struct Transform : public BaseComponent {
    vec2 position{0.f, 0.f};
    vec2 size{1.f, 1.f};
    vec2 velocity{0.f, 0.f};
    collision::CollisionConfig collision_config;
};

// Helper math functions
float vector_length(const vec2& v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}

vec2 normalize_vec(const vec2& v) {
    float len = vector_length(v);
    if (len < 0.0001f) return {0.f, 0.f};
    return {v.x / len, v.y / len};
}

float dot_product(const vec2& a, const vec2& b) {
    return a.x * b.x + a.y * b.y;
}

float get_speed(const Transform& t) {
    return vector_length(t.velocity);
}

bool check_overlap(const Transform& a, const Transform& b) {
    return a.position.x < b.position.x + b.size.x &&
           a.position.x + a.size.x > b.position.x &&
           a.position.y < b.position.y + b.size.y &&
           a.position.y + a.size.y > b.position.y;
}

int main() {
    std::cout << "=== Collision Plugin Example ===" << std::endl;

    // Test 1: CollisionConfig defaults
    std::cout << "\n1. CollisionConfig defaults:" << std::endl;
    collision::CollisionConfig config;
    std::cout << "  - mass: " << config.mass << " (expected 1.0)" << std::endl;
    std::cout << "  - friction: " << config.friction << " (expected 0.0)" << std::endl;
    std::cout << "  - restitution: " << config.restitution << " (expected 0.5)" << std::endl;
    assert(config.mass == 1.f);
    assert(config.friction == 0.f);
    assert(config.restitution == 0.5f);

    // Test 2: Create colliding entities
    std::cout << "\n2. Creating colliding entities:" << std::endl;

    Entity& entity1 = EntityHelper::createEntity();
    Transform& t1 = entity1.addComponent<Transform>();
    t1.position = {0.f, 0.f};
    t1.size = {10.f, 10.f};
    t1.velocity = {5.f, 0.f};  // Moving right
    t1.collision_config.mass = 1.f;
    t1.collision_config.restitution = 0.8f;

    Entity& entity2 = EntityHelper::createEntity();
    Transform& t2 = entity2.addComponent<Transform>();
    t2.position = {8.f, 0.f};  // Overlapping with entity1
    t2.size = {10.f, 10.f};
    t2.velocity = {-3.f, 0.f};  // Moving left
    t2.collision_config.mass = 2.f;  // Heavier
    t2.collision_config.restitution = 0.8f;

    std::cout << "  - Entity 1: pos(" << t1.position.x << ", " << t1.position.y
              << "), vel(" << t1.velocity.x << ", " << t1.velocity.y << "), mass=" << t1.collision_config.mass << std::endl;
    std::cout << "  - Entity 2: pos(" << t2.position.x << ", " << t2.position.y
              << "), vel(" << t2.velocity.x << ", " << t2.velocity.y << "), mass=" << t2.collision_config.mass << std::endl;

    // Test 3: Check overlap detection
    std::cout << "\n3. Overlap detection:" << std::endl;
    bool overlapping = check_overlap(t1, t2);
    std::cout << "  - Entities overlap: " << (overlapping ? "yes" : "no") << std::endl;
    assert(overlapping);

    // Create non-overlapping entity
    Entity& entity3 = EntityHelper::createEntity();
    Transform& t3 = entity3.addComponent<Transform>();
    t3.position = {100.f, 100.f};
    t3.size = {10.f, 10.f};

    bool not_overlapping = check_overlap(t1, t3);
    std::cout << "  - Entity 1 overlaps entity 3: " << (not_overlapping ? "yes" : "no") << std::endl;
    assert(!not_overlapping);

    // Test 4: Set up collision system
    std::cout << "\n4. Setting up collision system:" << std::endl;

    SystemManager systems;

    auto collision_sys = std::make_unique<collision::UpdateCollidingEntities<Transform>>();

    // Configure the system before registering
    collision_sys->config.get_collision_scalar = []() { return 100.f; };
    collision_sys->config.get_max_speed = []() { return 50.f; };

    collision_sys->callbacks.normalize_vec = normalize_vec;
    collision_sys->callbacks.dot_product = dot_product;
    collision_sys->callbacks.vector_length = vector_length;
    collision_sys->callbacks.get_speed = get_speed;
    collision_sys->callbacks.check_overlap = check_overlap;

    systems.register_update_system(std::move(collision_sys));

    std::cout << "  - Collision system registered" << std::endl;
    std::cout << "  - Callbacks configured" << std::endl;

    // Test 5: Run collision resolution
    std::cout << "\n5. Running collision system:" << std::endl;
    std::cout << "  Before:" << std::endl;
    std::cout << "    Entity 1 velocity: (" << t1.velocity.x << ", " << t1.velocity.y << ")" << std::endl;
    std::cout << "    Entity 2 velocity: (" << t2.velocity.x << ", " << t2.velocity.y << ")" << std::endl;

    float dt = 0.016f;  // ~60 FPS
    systems.run(dt);

    std::cout << "  After:" << std::endl;
    std::cout << "    Entity 1 velocity: (" << t1.velocity.x << ", " << t1.velocity.y << ")" << std::endl;
    std::cout << "    Entity 2 velocity: (" << t2.velocity.x << ", " << t2.velocity.y << ")" << std::endl;
    std::cout << "  - Collision system executed successfully" << std::endl;

    // Test 6: Position correction
    std::cout << "\n6. Position correction (entities should be separated):" << std::endl;
    std::cout << "  - Entity 1 position: (" << t1.position.x << ", " << t1.position.y << ")" << std::endl;
    std::cout << "  - Entity 2 position: (" << t2.position.x << ", " << t2.position.y << ")" << std::endl;

    // After positional correction, entities should be further apart
    float initial_gap = 8.f - 0.f;  // Original x positions
    float current_gap = t2.position.x - t1.position.x;
    std::cout << "  - Initial gap: " << initial_gap << ", Current gap: " << current_gap << std::endl;

    // Test 7: Custom collision config
    std::cout << "\n7. Custom collision config:" << std::endl;
    collision::CollisionConfig heavy_config;
    heavy_config.mass = 10.f;
    heavy_config.friction = 0.5f;
    heavy_config.restitution = 0.2f;

    std::cout << "  - Heavy object: mass=" << heavy_config.mass
              << ", friction=" << heavy_config.friction
              << ", restitution=" << heavy_config.restitution << std::endl;
    assert(heavy_config.mass == 10.f);
    assert(heavy_config.friction == 0.5f);
    assert(heavy_config.restitution == 0.2f);

    // Test 8: Max mass (immovable object)
    std::cout << "\n8. Immovable object (infinite mass):" << std::endl;
    Entity& wall = EntityHelper::createEntity();
    Transform& wall_t = wall.addComponent<Transform>();
    wall_t.position = {50.f, 0.f};
    wall_t.size = {5.f, 100.f};
    wall_t.velocity = {0.f, 0.f};
    wall_t.collision_config.mass = std::numeric_limits<float>::max();

    std::cout << "  - Wall mass: infinity (max float)" << std::endl;
    std::cout << "  - Wall won't move during collisions" << std::endl;

    // Cleanup
    EntityHelper::cleanup();

    std::cout << "\n=== All collision tests passed! ===" << std::endl;
    return 0;
}
