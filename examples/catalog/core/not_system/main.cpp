#include <cassert>
#include <iostream>
#include <vector>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../../../ah.h"
#include "../../shared/vector.h"

// System<Not<SkipMe>> visits every entity that does NOT have SkipMe
// (see the Not<> support in src/core/system.h).

namespace afterhours {

struct Exists : public BaseComponent {};
struct SkipMe : public BaseComponent {};

struct SkipsSkippable : System<Exists, Not<SkipMe>> {
  std::vector<int> visited;
  void for_each_with(Entity &entity, Exists &, float) override {
    if (entity.has<SkipMe>()) return;
    visited.push_back(entity.id);
    std::cout << "entity " << entity.id << std::endl;
  }
};

} // namespace afterhours

int main(int, char **) {
  using namespace afterhours;

  for (int i = 0; i < 5; i++) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Exists>();
    if (i % 2 == 0) entity.addComponent<SkipMe>();
  }
  EntityHelper::merge_entity_arrays();

  SystemManager systems;
  auto system = std::make_unique<SkipsSkippable>();
  SkipsSkippable &skips = *system;
  systems.register_update_system(std::move(system));
  systems.run(1.f);

  // Entities 1 and 3 are the ones without SkipMe.
  assert(skips.visited.size() == 2);
  assert(skips.visited[0] != skips.visited[1]);
  for (int id : skips.visited) assert(id % 2 == 1);
  std::cout << "visited " << skips.visited.size()
            << " entities without SkipMe" << std::endl;
  return 0;
}
