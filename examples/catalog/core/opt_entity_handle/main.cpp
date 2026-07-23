#include <cassert>
#include <iostream>

#define AFTER_HOURS_ENTITY_HELPER
#include "../../../ah.h"
#include "../../../src/core/opt_entity_handle.h"

using namespace afterhours;

struct Position : BaseComponent {
  float x = 0.0f;
  float y = 0.0f;
  Position() = default;
  Position(float x_, float y_) : x(x_), y(y_) {}
};

int main() {
  std::cout << "=== OptEntityHandle Example ===" << std::endl;

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // Create an entity and capture an optional handle.
  Entity &entity = EntityHelper::createEntity();
  entity.addComponent<Position>(Position(3.0f, 4.0f));
  EntityHelper::merge_entity_arrays();

  OptEntityHandle handle = OptEntityHandle::from_entity(entity);
  OptEntity resolved = handle.resolve();

  std::cout << "Resolved handle valid: " << (resolved.valid() ? "yes" : "no")
            << std::endl;
  assert(resolved.valid());
  assert(resolved.asE().get<Position>().x == 3.0f);
  assert(resolved.asE().get<Position>().y == 4.0f);

  // Deleting all entities should invalidate the handle resolution.
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  OptEntity invalid = handle.resolve();
  std::cout << "Resolved after delete valid: "
            << (invalid.valid() ? "yes" : "no") << std::endl;
  assert(!invalid.valid());

  std::cout << "Example completed successfully!" << std::endl;
  return 0;
}

