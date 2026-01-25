#include <cassert>
#include <iostream>
#include <string>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"

using namespace afterhours;

// Tags for entity categorization
enum struct UnitTag : uint8_t { Player = 0, Enemy = 1, NPC = 2, Projectile = 3 };

// Components
struct Position : BaseComponent {
    float x, y;
    Position(float x_ = 0.0f, float y_ = 0.0f) : x(x_), y(y_) {}
};

struct Velocity : BaseComponent {
    float dx, dy;
    Velocity(float dx_ = 0.0f, float dy_ = 0.0f) : dx(dx_), dy(dy_) {}
};

struct Health : BaseComponent {
    int current;
    int max;
    Health(int c = 100, int m = 100) : current(c), max(m) {}
};

struct Frozen : BaseComponent {
    int turns_remaining;
    Frozen(int turns = 3) : turns_remaining(turns) {}
};

// Global counters for tracking system execution
static int movement_updates = 0;
static int enemy_updates = 0;
static int callback_calls = 0;
static int fixed_updates = 0;
static int render_calls = 0;

// Test 1: Basic System with single component
struct MovementSystem : System<Position, Velocity> {
    virtual void for_each_with(Entity&, Position& pos, Velocity& vel, float dt) override {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
        movement_updates++;
    }
};

// Note: Not<T> is declared in system.h but not fully implemented
// See not_system example for details - the feature is commented out

// Test 2: System with once() and after() hooks
struct LifecycleTestSystem : System<Position> {
    mutable int once_count = 0;
    mutable int for_each_count = 0;
    mutable int after_count = 0;

    virtual void once(const float) override {
        once_count++;
    }

    virtual void for_each_with(Entity&, Position&, float) override {
        for_each_count++;
    }

    virtual void after(const float) override {
        after_count++;
    }
};

// Test 3: System with should_run() conditional execution
struct ConditionalSystem : System<Position> {
    mutable int call_count = 0;
    bool enabled = true;

    virtual bool should_run(const float) override {
        return enabled;
    }

    virtual void for_each_with(Entity&, Position&, float) override {
        call_count++;
    }
};

// Test 4: System with tag filtering using tags::All
// Only runs on entities with Enemy tag
struct EnemyOnlySystem : System<Health, tags::All<UnitTag::Enemy>> {
    virtual void for_each_with(Entity&, Health& health, float) override {
        // Simulate damage tick
        health.current -= 1;
        enemy_updates++;
    }
};

// Test 5: Fixed update system for physics
struct FixedPhysicsSystem : System<Position, Velocity> {
    virtual void for_each_with(Entity&, Position& pos, Velocity& vel, float dt) override {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
        fixed_updates++;
    }
};

// Test 6: Render system (const correctness)
struct RenderSystem : System<Position> {
    virtual void for_each_with(const Entity&, const Position&, float) const override {
        render_calls++;
    }
};

int main() {
    std::cout << "=== System Example ===" << std::endl;

    // Test 1: Basic system with component filtering
    std::cout << "\n1. Basic System with component filtering..." << std::endl;
    {
        // Reset
        movement_updates = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        // Create entities: some with velocity, some without
        auto& e1 = EntityHelper::createEntity();
        e1.addComponent<Position>(0.0f, 0.0f);
        e1.addComponent<Velocity>(10.0f, 0.0f);

        auto& e2 = EntityHelper::createEntity();
        e2.addComponent<Position>(100.0f, 0.0f);
        // No velocity - should not be processed

        auto& e3 = EntityHelper::createEntity();
        e3.addComponent<Position>(0.0f, 0.0f);
        e3.addComponent<Velocity>(0.0f, 5.0f);

        EntityHelper::merge_entity_arrays();

        SystemManager systems;
        systems.register_update_system(std::make_unique<MovementSystem>());
        systems.run(1.0f);

        std::cout << "  Entities processed: " << movement_updates << " (expected 2)" << std::endl;
        assert(movement_updates == 2);

        // Verify positions updated
        auto query = EntityQuery<>().whereHasComponent<Position>().whereHasComponent<Velocity>().gen();
        for (auto& ref : query) {
            Position& pos = ref.get().get<Position>();
            std::cout << "  Entity position: (" << pos.x << ", " << pos.y << ")" << std::endl;
        }
    }

    // Test 2: System with Not<T> exclusion
    // Note: Not<T> is declared in system.h but not fully implemented
    // See not_system example for details - this test is skipped
    std::cout << "\n2. System with Not<T> exclusion..." << std::endl;
    std::cout << "  (Skipped - Not<T> feature not fully implemented)" << std::endl;

    // Test 3: System lifecycle hooks (once, for_each, after)
    std::cout << "\n3. System lifecycle hooks..." << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 5; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i), 0.0f);
        }
        EntityHelper::merge_entity_arrays();

        auto lifecycle_system = std::make_unique<LifecycleTestSystem>();
        auto* sys_ptr = lifecycle_system.get();

        SystemManager systems;
        systems.register_update_system(std::move(lifecycle_system));

        // Run once
        systems.run(1.0f);
        std::cout << "  After 1 tick:" << std::endl;
        std::cout << "    once() called: " << sys_ptr->once_count << " time(s)" << std::endl;
        std::cout << "    for_each() called: " << sys_ptr->for_each_count << " time(s)" << std::endl;
        std::cout << "    after() called: " << sys_ptr->after_count << " time(s)" << std::endl;
        assert(sys_ptr->once_count == 1);
        assert(sys_ptr->for_each_count == 5);
        assert(sys_ptr->after_count == 1);

        // Run again
        systems.run(1.0f);
        std::cout << "  After 2 ticks:" << std::endl;
        std::cout << "    once() total: " << sys_ptr->once_count << std::endl;
        std::cout << "    for_each() total: " << sys_ptr->for_each_count << std::endl;
        std::cout << "    after() total: " << sys_ptr->after_count << std::endl;
        assert(sys_ptr->once_count == 2);
        assert(sys_ptr->for_each_count == 10);
        assert(sys_ptr->after_count == 2);
    }

    // Test 4: should_run() conditional execution
    std::cout << "\n4. Conditional system with should_run()..." << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& e = EntityHelper::createEntity();
        e.addComponent<Position>(0.0f, 0.0f);
        EntityHelper::merge_entity_arrays();

        auto conditional = std::make_unique<ConditionalSystem>();
        auto* sys_ptr = conditional.get();

        SystemManager systems;
        systems.register_update_system(std::move(conditional));

        // Run with enabled = true
        sys_ptr->enabled = true;
        systems.run(1.0f);
        std::cout << "  Enabled=true, calls: " << sys_ptr->call_count << std::endl;
        assert(sys_ptr->call_count == 1);

        // Run with enabled = false
        sys_ptr->enabled = false;
        systems.run(1.0f);
        std::cout << "  Enabled=false, calls: " << sys_ptr->call_count << " (unchanged)" << std::endl;
        assert(sys_ptr->call_count == 1);  // Should not have increased

        // Re-enable
        sys_ptr->enabled = true;
        systems.run(1.0f);
        std::cout << "  Enabled=true again, calls: " << sys_ptr->call_count << std::endl;
        assert(sys_ptr->call_count == 2);
    }

    // Test 5: Tag-based filtering with tags::All
    std::cout << "\n5. Tag-based filtering with tags::All..." << std::endl;
    {
        enemy_updates = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        // Create player
        auto& player = EntityHelper::createEntity();
        player.addComponent<Health>(100, 100);
        player.enableTag(UnitTag::Player);

        // Create enemies
        auto& enemy1 = EntityHelper::createEntity();
        enemy1.addComponent<Health>(50, 50);
        enemy1.enableTag(UnitTag::Enemy);

        auto& enemy2 = EntityHelper::createEntity();
        enemy2.addComponent<Health>(30, 30);
        enemy2.enableTag(UnitTag::Enemy);

        // Create NPC
        auto& npc = EntityHelper::createEntity();
        npc.addComponent<Health>(20, 20);
        npc.enableTag(UnitTag::NPC);

        EntityHelper::merge_entity_arrays();

        SystemManager systems;
        systems.register_update_system(std::make_unique<EnemyOnlySystem>());
        systems.run(1.0f);

        std::cout << "  Enemy entities updated: " << enemy_updates << " (expected 2)" << std::endl;
        assert(enemy_updates == 2);  // Only enemies should be processed
    }

    // Test 6: CallbackSystem for quick lambdas
    std::cout << "\n6. CallbackSystem for lambda-based systems..." << std::endl;
    {
        callback_calls = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& e = EntityHelper::createEntity();
        e.addComponent<Position>(0.0f, 0.0f);
        EntityHelper::merge_entity_arrays();

        SystemManager systems;
        systems.register_update_system([](float dt) {
            callback_calls++;
            std::cout << "    Lambda callback executed with dt=" << dt << std::endl;
        });

        systems.run(1.0f);
        systems.run(1.0f);

        std::cout << "  Callback invocations: " << callback_calls << " (expected 2)" << std::endl;
        assert(callback_calls == 2);
    }

    // Test 7: Fixed update vs regular update
    std::cout << "\n7. Fixed update system..." << std::endl;
    {
        fixed_updates = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& e = EntityHelper::createEntity();
        e.addComponent<Position>(0.0f, 0.0f);
        e.addComponent<Velocity>(1.0f, 0.0f);
        EntityHelper::merge_entity_arrays();

        SystemManager systems;
        systems.register_fixed_update_system(std::make_unique<FixedPhysicsSystem>());

        // Fixed tick rate is 1/120 seconds
        // Running with dt=0.1 should trigger multiple fixed updates
        float dt = 0.1f;
        systems.run(dt);

        std::cout << "  Fixed updates with dt=" << dt << ": " << fixed_updates << std::endl;
        // Should have multiple fixed updates since dt > FIXED_TICK_RATE (1/120)
        assert(fixed_updates > 0);
    }

    // Test 8: Render system (const correctness)
    std::cout << "\n8. Render system (const entities)..." << std::endl;
    {
        render_calls = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 3; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i * 10), 0.0f);
        }
        EntityHelper::merge_entity_arrays();

        SystemManager systems;
        systems.register_render_system(std::make_unique<RenderSystem>());
        systems.run(1.0f);

        std::cout << "  Render calls: " << render_calls << " (expected 3)" << std::endl;
        assert(render_calls == 3);
    }

    // Test 9: Multiple systems in order
    std::cout << "\n9. Multiple systems execute in registration order..." << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& e = EntityHelper::createEntity();
        e.addComponent<Position>(0.0f, 0.0f);
        EntityHelper::merge_entity_arrays();

        std::vector<int> execution_order;

        SystemManager systems;
        systems.register_update_system([&execution_order](float) {
            execution_order.push_back(1);
        });
        systems.register_update_system([&execution_order](float) {
            execution_order.push_back(2);
        });
        systems.register_update_system([&execution_order](float) {
            execution_order.push_back(3);
        });

        systems.run(1.0f);

        std::cout << "  Execution order: ";
        for (int i : execution_order) std::cout << i << " ";
        std::cout << std::endl;

        assert(execution_order.size() == 3);
        assert(execution_order[0] == 1);
        assert(execution_order[1] == 2);
        assert(execution_order[2] == 3);
    }

    // Cleanup
    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

    std::cout << "\n=== All System tests passed! ===" << std::endl;
    return 0;
}
