
#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"

#include "../tag_filter_regression/demo_tags.h"
#include "../../src/ecs.h"

namespace afterhours {

TEST_CASE("ECS: pooled component storage preserves add/get/has/remove API",
          "[ECS][Components][Pools]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  Entity &c = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  REQUIRE_FALSE(a.has<TagTestTransform>());
  REQUIRE_FALSE(b.has<TagTestTransform>());
  REQUIRE_FALSE(c.has<TagTestTransform>());

  a.addComponent<TagTestTransform>().x = 10;
  b.addComponent<TagTestTransform>().x = 20;
  c.addComponent<TagTestTransform>().x = 30;

  REQUIRE(a.has<TagTestTransform>());
  REQUIRE(b.has<TagTestTransform>());
  REQUIRE(c.has<TagTestTransform>());
  REQUIRE(a.get<TagTestTransform>().x == 10);
  REQUIRE(b.get<TagTestTransform>().x == 20);
  REQUIRE(c.get<TagTestTransform>().x == 30);

  b.removeComponent<TagTestTransform>();
  REQUIRE_FALSE(b.has<TagTestTransform>());

  // Swap-remove correctness: removing b's component should not corrupt others.
  REQUIRE(a.get<TagTestTransform>().x == 10);
  REQUIRE(c.get<TagTestTransform>().x == 30);
}

TEST_CASE("ECS: derived child queries work with pooled components",
          "[ECS][Components][Derived]") {
  struct BaseFoo : BaseComponent {
    int v = 1;
  };
  struct DerivedFoo : BaseFoo {
    int extra = 7;
  };

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  Entity &e = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  e.addComponent<DerivedFoo>().v = 42;

  REQUIRE(e.has_child_of<BaseFoo>());
  BaseFoo &as_base = e.get_with_child<BaseFoo>();
  REQUIRE(as_base.v == 42);
}

} // namespace afterhours

