#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../ah.h"

using namespace afterhours;

struct Position : BaseComponent {
    float x, y;
    Position(float x_ = 0.f, float y_ = 0.f) : x(x_), y(y_) {}
};

struct Velocity : BaseComponent {
    float dx, dy;
    Velocity(float dx_ = 0.f, float dy_ = 0.f) : dx(dx_), dy(dy_) {}
};

enum struct UnitTag : uint8_t { Player = 0, Enemy = 1 };

static int once_only_count = 0;
static int iterate_count = 0;
static int for_each_count = 0;
static int callback_count = 0;
static int tagged_count = 0;
static int untagged_count = 0;

struct OnceOnlySystem : System<> {
    bool should_iterate() const override { return false; }
    void once(float) override { once_only_count++; }
};

struct IteratingSystem : System<Position> {
    void for_each_with(Entity&, Position&, float) override { iterate_count++; }
};

struct DefaultSystem : System<> {
    void for_each_with(Entity&, float) override { for_each_count++; }
};

struct TaggedSystem : System<Position, tags::All<UnitTag::Enemy>> {
    void for_each_with(Entity&, Position&, float) override { tagged_count++; }
};

struct UntaggedSystem : System<Position> {
    void for_each_with(Entity&, Position&, float) override { untagged_count++; }
};

int main() {
    std::cout << "=== should_iterate() Tests ===" << std::endl;

    std::cout << "\n1. System with should_iterate()=false skips entity loop..."
              << std::endl;
    {
        once_only_count = 0;
        iterate_count = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 10; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i), 0.f);
        }
        EntityHelper::merge_entity_arrays();

        SystemManager sm;
        sm.register_update_system(std::make_unique<OnceOnlySystem>());
        sm.register_update_system(std::make_unique<IteratingSystem>());
        sm.run(1.f);

        std::cout << "  once_only_count: " << once_only_count << " (expected 1)"
                  << std::endl;
        std::cout << "  iterate_count: " << iterate_count << " (expected 10)"
                  << std::endl;
        assert(once_only_count == 1);
        assert(iterate_count == 10);
    }

    std::cout << "\n2. Default System<> still iterates all entities..."
              << std::endl;
    {
        for_each_count = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 5; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i), 0.f);
        }
        EntityHelper::merge_entity_arrays();

        SystemManager sm;
        sm.register_update_system(std::make_unique<DefaultSystem>());
        sm.run(1.f);

        std::cout << "  for_each_count: " << for_each_count << " (expected 5)"
                  << std::endl;
        assert(for_each_count == 5);
    }

    std::cout << "\n3. CallbackSystem skips iteration..." << std::endl;
    {
        callback_count = 0;
        for_each_count = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 5; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i), 0.f);
        }
        EntityHelper::merge_entity_arrays();

        SystemManager sm;
        sm.register_update_system([](float) { callback_count++; });
        sm.register_update_system(std::make_unique<DefaultSystem>());
        sm.run(1.f);

        std::cout << "  callback_count: " << callback_count << " (expected 1)"
                  << std::endl;
        std::cout << "  for_each_count: " << for_each_count << " (expected 5)"
                  << std::endl;
        assert(callback_count == 1);
        assert(for_each_count == 5);
    }

    std::cout << "\n4. should_iterate()=false still calls once() and after()..."
              << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& e = EntityHelper::createEntity();
        e.addComponent<Position>(1.f, 2.f);
        EntityHelper::merge_entity_arrays();

        int once_calls = 0;
        int after_calls = 0;
        int fe_calls = 0;

        struct FullLifecycleNoIterate : System<Position> {
            int& once_ref;
            int& after_ref;
            int& fe_ref;
            FullLifecycleNoIterate(int& o, int& a, int& f)
                : once_ref(o), after_ref(a), fe_ref(f) {}
            bool should_iterate() const override { return false; }
            void once(float) override { once_ref++; }
            void after(float) override { after_ref++; }
            void for_each_with(Entity&, Position&, float) override { fe_ref++; }
        };

        SystemManager sm;
        sm.register_update_system(std::make_unique<FullLifecycleNoIterate>(
            once_calls, after_calls, fe_calls));
        sm.run(1.f);
        sm.run(1.f);

        std::cout << "  once: " << once_calls << " (expected 2)" << std::endl;
        std::cout << "  after: " << after_calls << " (expected 2)" << std::endl;
        std::cout << "  for_each: " << fe_calls << " (expected 0)" << std::endl;
        assert(once_calls == 2);
        assert(after_calls == 2);
        assert(fe_calls == 0);
    }

    std::cout << "\n5. has_tag_requirements constexpr correctness..."
              << std::endl;
    {
        static_assert(!System<Position>::has_tag_requirements,
                      "System<Position> should have no tag requirements");
        static_assert(
            !System<Position, Velocity>::has_tag_requirements,
            "System<Position, Velocity> should have no tag requirements");
        static_assert(!System<>::has_tag_requirements,
                      "System<> should have no tag requirements");
        static_assert(
            System<Position, tags::All<UnitTag::Enemy>>::has_tag_requirements,
            "System with tags::All should have tag requirements");
        static_assert(System<tags::Any<UnitTag::Player,
                                       UnitTag::Enemy>>::has_tag_requirements,
                      "System with tags::Any should have tag requirements");
        static_assert(
            System<Position, tags::None<UnitTag::Enemy>>::has_tag_requirements,
            "System with tags::None should have tag requirements");

        std::cout << "  All static_asserts passed." << std::endl;
    }

    std::cout << "\n6. Tagged system still works with constexpr tags_ok "
                 "optimization..."
              << std::endl;
    {
        tagged_count = 0;
        untagged_count = 0;
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        auto& player = EntityHelper::createEntity();
        player.addComponent<Position>(0.f, 0.f);
        player.enableTag(UnitTag::Player);

        auto& enemy1 = EntityHelper::createEntity();
        enemy1.addComponent<Position>(1.f, 0.f);
        enemy1.enableTag(UnitTag::Enemy);

        auto& enemy2 = EntityHelper::createEntity();
        enemy2.addComponent<Position>(2.f, 0.f);
        enemy2.enableTag(UnitTag::Enemy);

        auto& plain = EntityHelper::createEntity();
        plain.addComponent<Position>(3.f, 0.f);

        EntityHelper::merge_entity_arrays();

        SystemManager sm;
        sm.register_update_system(std::make_unique<TaggedSystem>());
        sm.register_update_system(std::make_unique<UntaggedSystem>());
        sm.run(1.f);

        std::cout << "  tagged_count (enemy only): " << tagged_count
                  << " (expected 2)" << std::endl;
        std::cout << "  untagged_count (all with Position): " << untagged_count
                  << " (expected 4)" << std::endl;
        assert(tagged_count == 2);
        assert(untagged_count == 4);
    }

    std::cout << "\n7. should_iterate in render and fixed_update systems..."
              << std::endl;
    {
        EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

        for (int i = 0; i < 3; i++) {
            auto& e = EntityHelper::createEntity();
            e.addComponent<Position>(static_cast<float>(i), 0.f);
        }
        EntityHelper::merge_entity_arrays();

        int render_once = 0;
        int render_fe = 0;
        int fixed_once = 0;
        int fixed_fe = 0;

        struct RenderOnceOnly : System<Position> {
            int& once_ref;
            int& fe_ref;
            RenderOnceOnly(int& o, int& f) : once_ref(o), fe_ref(f) {}
            bool should_iterate() const override { return false; }
            void once(float) override { once_ref++; }
            void for_each_with(const Entity&, const Position&,
                               float) const override {
                fe_ref++;
            }
        };

        struct FixedOnceOnly : System<Position> {
            int& once_ref;
            int& fe_ref;
            FixedOnceOnly(int& o, int& f) : once_ref(o), fe_ref(f) {}
            bool should_iterate() const override { return false; }
            void once(float) override { once_ref++; }
            void for_each_with(Entity&, Position&, float) override { fe_ref++; }
        };

        SystemManager sm;
        sm.register_render_system(
            std::make_unique<RenderOnceOnly>(render_once, render_fe));
        sm.register_fixed_update_system(
            std::make_unique<FixedOnceOnly>(fixed_once, fixed_fe));
        sm.run(0.1f);

        std::cout << "  render once: " << render_once
                  << ", for_each: " << render_fe << std::endl;
        std::cout << "  fixed once: " << fixed_once
                  << ", for_each: " << fixed_fe << std::endl;
        assert(render_once >= 1);
        assert(render_fe == 0);
        assert(fixed_once >= 1);
        assert(fixed_fe == 0);
    }

    EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

    std::cout << "\n=== All should_iterate() tests passed! ===" << std::endl;
    return 0;
}
