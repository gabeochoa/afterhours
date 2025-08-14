#include <iostream>

#include "../../ah.h"

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
  {
    auto &e = EntityHelper::createEntity();
    e.addComponent<Transform>().x = 0;
    e.enableTag(DemoTag::Runner);
  }
  {
    auto &e = EntityHelper::createEntity();
    e.addComponent<Transform>().x = 5;
    e.enableTag(DemoTag::Runner);
    e.enableTag(DemoTag::Store); // should be excluded from MoveRunners and
                                 // DebugNonStore
  }
  {
    auto &e = EntityHelper::createEntity();
    e.addComponent<Health>().hp = 50;
    e.enableTag(DemoTag::Chaser);
  }
  {
    auto &e = EntityHelper::createEntity();
    e.addComponent<Health>().hp = 10;
    e.enableTag(DemoTag::Runner);
  }

  SystemManager sm;
  sm.register_update_system(std::make_unique<MoveRunners>());
  sm.register_update_system(std::make_unique<HealAnyoneTagged>());
  sm.register_update_system(std::make_unique<DebugNonStore>());

  // Run two ticks
  sm.tick_all(ents, 0.016f);
  sm.tick_all(ents, 0.016f);

  // Print results
  for (auto &sp : ents) {
    if (!sp)
      continue;
    Entity &e = *sp;
    if (e.has<Transform>()) {
      std::cout << "Entity " << e.id << " Transform.x=" << e.get<Transform>().x
                << (e.hasTag((TagId)DemoTag::Runner) ? " [Runner]" : "")
                << (e.hasTag((TagId)DemoTag::Store) ? " [Store]" : "") << "\n";
    }
    if (e.has<Health>()) {
      std::cout << "Entity " << e.id << " Health.hp=" << e.get<Health>().hp
                << (e.hasTag((TagId)DemoTag::Runner) ? " [Runner]" : "")
                << (e.hasTag((TagId)DemoTag::Chaser) ? " [Chaser]" : "")
                << "\n";
    }
  }

  return 0;
}
