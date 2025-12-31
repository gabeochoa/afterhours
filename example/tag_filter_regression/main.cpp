#include <iostream>

#include "../../src/ecs.h"

#include "demo_tags.h"

using namespace afterhours;

int main() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  a.enableTag(DemoTag::Runner);
  a.addComponent<TagTestTransform>().x = 1;

  Entity &b = EntityHelper::createEntity();
  b.enableTag(DemoTag::Runner);
  b.enableTag(DemoTag::Store);
  b.addComponent<TagTestTransform>().x = 5;

  Entity &c = EntityHelper::createEntity();
  c.enableTag(DemoTag::Chaser);
  c.addComponent<TagTestHealth>().hp = 50;

  EntityHelper::merge_entity_arrays();

  auto non_store =
      EntityQuery<>({.ignore_temp_warning = true}).whereHasNoTags(DemoTag::Store).gen();

  std::cout << "non-store count: " << non_store.size() << "\n";
  for (Entity &e : non_store) {
    std::cout << " - entity " << e.id << "\n";
  }

  return 0;
}

