#include <cassert>
#include <iostream>

#include "../../../../ah.h"

using namespace afterhours;

// Define a small tag enum for the demo
enum struct DemoTag : uint8_t { Runner = 0, Chaser = 1, Store = 2 };

struct Transform : BaseComponent {
    int x = 0;
};

struct Health : BaseComponent {
    int hp = 100;
};

// System requiring Transform and the Runner tag
struct MoveRunners : System<Transform, tags::All<DemoTag::Runner>,
                            tags::None<DemoTag::Store>> {
    void for_each_with(Entity &, Transform &t, float) override { t.x += 1; }
};

// System that runs on AI (Health) with any of the Chaser or Runner tags
struct HealAnyoneTagged
    : System<Health, tags::Any<DemoTag::Chaser, DemoTag::Runner>> {
    void for_each_with(Entity &, Health &h, float) override {
        h.hp = std::min(h.hp + 5, 100);
    }
};

// System that excludes Store-tagged entities regardless of components
struct DebugNonStore : System<tags::None<DemoTag::Store>> {
    void for_each_with(Entity &e, float) override {
        std::cout << "Debug entity id=" << e.id << " (non-store)\n";
    }
};

int main() {
    Entities &ents = EntityHelper::get_entities_for_mod();

    // Create sample entities
    auto &runner = EntityHelper::createEntity();
    runner.addComponent<Transform>().x = 0;
    runner.enableTag(DemoTag::Runner);

    auto &store_runner = EntityHelper::createEntity();
    store_runner.addComponent<Transform>().x = 5;
    store_runner.enableTag(DemoTag::Runner);
    store_runner.enableTag(DemoTag::Store);  // excluded from MoveRunners

    auto &chaser = EntityHelper::createEntity();
    chaser.addComponent<Health>().hp = 50;
    chaser.enableTag(DemoTag::Chaser);

    auto &hurt_runner = EntityHelper::createEntity();
    hurt_runner.addComponent<Health>().hp = 10;
    hurt_runner.enableTag(DemoTag::Runner);

    SystemManager sm;
    sm.register_update_system(std::make_unique<MoveRunners>());
    sm.register_update_system(std::make_unique<HealAnyoneTagged>());
    sm.register_update_system(std::make_unique<DebugNonStore>());

    // Run two ticks
    sm.tick_all(ents, 0.016f);
    sm.tick_all(ents, 0.016f);

    // Print results
    for (auto &sp : ents) {
        if (!sp) continue;
        Entity &e = *sp;
        if (e.has<Transform>()) {
            std::cout << "Entity " << e.id
                      << " Transform.x=" << e.get<Transform>().x
                      << (e.hasTag((TagId) DemoTag::Runner) ? " [Runner]" : "")
                      << (e.hasTag((TagId) DemoTag::Store) ? " [Store]" : "")
                      << "\n";
        }
        if (e.has<Health>()) {
            std::cout << "Entity " << e.id
                      << " Health.hp=" << e.get<Health>().hp
                      << (e.hasTag((TagId) DemoTag::Runner) ? " [Runner]" : "")
                      << (e.hasTag((TagId) DemoTag::Chaser) ? " [Chaser]" : "")
                      << "\n";
        }
    }

    // MoveRunners moved only the non-Store Runner; both Health entities
    // matching Any<Chaser, Runner> healed on both ticks.
    assert(runner.get<Transform>().x == 1);
    assert(store_runner.get<Transform>().x == 5);
    assert(chaser.get<Health>().hp == 60);
    assert(hurt_runner.get<Health>().hp == 20);
    return 0;
}
