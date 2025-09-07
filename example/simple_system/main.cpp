

#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include "../../ah.h"
#include "../shared/vector.h"

namespace afterhours {

// Make some components :)
struct Transform : public BaseComponent {
  Transform(float x, float y) {
    position.x = x;
    position.y = y;
  }

  virtual ~Transform() {}

  [[nodiscard]] vec2 pos() const { return position; }
  void update(vec2 v) { position = v; }

private:
  vec2 position;
};

struct HasName : public BaseComponent {
  std::string name;
  HasName(std::string s) : name(s) {}
  virtual ~HasName() {}
};

struct Moves : System<Transform> {
  virtual ~Moves() {}
  // notice how the system<> template class is given here
  // so we will have access to the underlying
  virtual void for_each_with(Entity &entity, Transform &transform,
                             float) override {
    auto p = transform.pos() + vec2{1, 0};
    transform.update(p);
    std::cout << " updating for entity " << entity.id << " "
              << transform.pos().x << std::endl;
  }
};

struct MovesAndHasName : System<Transform, HasName> {
  virtual ~MovesAndHasName() {}
  virtual void for_each_with(Entity &entity, Transform &transform,
                             HasName &hasName, float) override {
    auto p = transform.pos() + vec2{1, 0};
    transform.update(p);
    std::cout << " updating for " << hasName.name << " with " << entity.id
              << " " << transform.pos().x << std::endl;
  }
};

} // namespace afterhours

void make_entities() {
  using namespace afterhours;

  for (int i = 0; i < 5; i++) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Transform>(0, 10);
  }

  for (int i = 0; i < 5; i++) {
    auto &entity = EntityHelper::createEntity();
    entity.addComponent<Transform>(0, 10);
    entity.addComponent<HasName>("my name");
  }
}

void test_component_sets() {
  using namespace afterhours;

  // Test component set queries directly
  auto transform_entities = EntityHelper::intersect_components<Transform>();
  auto hasname_entities = EntityHelper::intersect_components<HasName>();
  auto both_entities = EntityHelper::intersect_components<Transform, HasName>();

  std::cout << "Transform entities: " << transform_entities.size() << std::endl;
  std::cout << "HasName entities: " << hasname_entities.size() << std::endl;
  std::cout << "Both components entities: " << both_entities.size()
            << std::endl;

  // Assertions to verify component sets are working
  assert(transform_entities.size() == 10 &&
         "Should have 10 entities with Transform component");
  assert(hasname_entities.size() == 5 &&
         "Should have 5 entities with HasName component");
  assert(both_entities.size() == 5 &&
         "Should have 5 entities with both components");

  std::cout << "✓ Component set tests passed!" << std::endl;
}

int main(int, char **) {
  using namespace afterhours;

  make_entities();

  // Merge entities from temp to main list before testing
  EntityHelper::merge_entity_arrays();

  // Rebuild component sets after merging
  EntityHelper::rebuild_component_sets();

  // Test component sets before running systems
  test_component_sets();

  SystemManager systems;
  systems.register_update_system(std::make_unique<Moves>());
  systems.register_update_system(std::make_unique<MovesAndHasName>());

  for (int i = 0; i < 2; i++) {
    systems.run(1.f);
  }

  return 0;
}
