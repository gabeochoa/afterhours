#include "../../vendor/catch2/catch.hpp"
//
#include "../../src/ecs.h"

namespace afterhours {

static void reset_ecs_world_for_test() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST_CASE("EntityHandle - invalid before merge, resolves after merge",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  Entity &e = EntityHelper::createEntity();
  EntityHandle pre = EntityHelper::handle_for(e);
#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
  REQUIRE(pre.valid());
  REQUIRE(EntityHelper::resolve(pre).asE().id == e.id);
#else
  REQUIRE_FALSE(pre.valid());
#endif

  EntityHelper::merge_entity_arrays();
  EntityHandle h = EntityHelper::handle_for(e);
  REQUIRE(h.valid());
  auto opt = EntityHelper::resolve(h);
  REQUIRE(opt);
  REQUIRE(opt.asE().id == e.id);
}

TEST_CASE("EntityHandle - temp entity cleaned before merge invalidates handle (opt-in)",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  Entity &e = EntityHelper::createEntity();
  EntityHandle h = EntityHelper::handle_for(e);

#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
  REQUIRE(h.valid());
  e.cleanup = true;
  EntityHelper::merge_entity_arrays(); // should skip + invalidate
  REQUIRE_FALSE(EntityHelper::resolve(h).valid());
#else
  (void)h;
  SUCCEED();
#endif
}

TEST_CASE("EntityHandle - handle can resolve but query still misses temp entities",
          "[EntityHandle][EntityQuery]") {
  reset_ecs_world_for_test();

  Entity &e = EntityHelper::createEntity();
  const int id = e.id;
  const EntityHandle h = EntityHelper::handle_for(e);

#if defined(AFTER_HOURS_ASSIGN_HANDLES_ON_CREATE)
  REQUIRE(h.valid());
  REQUIRE(EntityHelper::resolve(h).asE().id == id);
#else
  REQUIRE_FALSE(h.valid());
  REQUIRE_FALSE(EntityHelper::resolve(h).valid());
#endif

  // Queries should still miss temp entities unless force-merged.
  {
    EntityQuery<> q({.ignore_temp_warning = true});
    q.whereID(id);
    REQUIRE_FALSE(q.has_values());
  }

  {
    EntityQuery<> q({.force_merge = true, .ignore_temp_warning = true});
    q.whereID(id);
    REQUIRE(q.has_values());
  }
}

TEST_CASE("EntityHandle - stale handle fails after cleanup + slot reuse bumps gen",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  Entity &e = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  EntityHandle h1 = EntityHelper::handle_for(e);
  REQUIRE(h1.valid());

  // Delete the entity; do not touch `e` after this (it may be destroyed).
  EntityHelper::markIDForCleanup(e.id);
  EntityHelper::cleanup();

  REQUIRE_FALSE(EntityHelper::resolve(h1).valid());

  // Create a new entity; it should reuse the freed slot and bump generation.
  Entity &e2 = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  EntityHandle h2 = EntityHelper::handle_for(e2);
  REQUIRE(h2.valid());

  REQUIRE(h2.slot == h1.slot);
  REQUIRE(h2.gen != h1.gen);
  REQUIRE(EntityHelper::resolve(h2).asE().id == e2.id);
  REQUIRE_FALSE(EntityHelper::resolve(h1).valid());
}

TEST_CASE("EntityHandle - swap-removal keeps remaining handles valid",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  Entity &c = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  EntityHandle ha = EntityHelper::handle_for(a);
  EntityHandle hb = EntityHelper::handle_for(b);
  EntityHandle hc = EntityHelper::handle_for(c);
  REQUIRE(ha.valid());
  REQUIRE(hb.valid());
  REQUIRE(hc.valid());

  // Remove the middle entity. This should swap-remove in the dense array.
  EntityHelper::markIDForCleanup(b.id);
  EntityHelper::cleanup();

  REQUIRE_FALSE(EntityHelper::resolve(hb).valid());
  REQUIRE(EntityHelper::resolve(ha).asE().id == a.id);
  REQUIRE(EntityHelper::resolve(hc).asE().id == c.id);
}

TEST_CASE("EntityHandle - delete_all_entities keeps permanent handles valid",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  Entity &perm = EntityHelper::createPermanentEntity();
  Entity &tmp = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  EntityHandle hperm = EntityHelper::handle_for(perm);
  EntityHandle htmp = EntityHelper::handle_for(tmp);
  REQUIRE(hperm.valid());
  REQUIRE(htmp.valid());

  EntityHelper::delete_all_entities(false);

  REQUIRE(EntityHelper::resolve(hperm).asE().id == perm.id);
  REQUIRE_FALSE(EntityHelper::resolve(htmp).valid());
}

TEST_CASE("EntityHandle - resolving invalid handle returns empty",
          "[EntityHandle]") {
  reset_ecs_world_for_test();
  REQUIRE_FALSE(EntityHelper::resolve(EntityHandle::invalid()).valid());
}

TEST_CASE("EntityHandle - churn reuse bumps generation and stale never resolves",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  // Allocate one entity and get its handle.
  Entity &e0 = EntityHelper::createEntity();
  const int live_id0 = e0.id;
  EntityHelper::merge_entity_arrays();
  EntityHandle h = EntityHelper::handle_for(e0);
  REQUIRE(h.valid());

  // Repeatedly delete and recreate to force slot reuse + generation bumps.
  int live_id = live_id0;
  for (int i = 0; i < 200; ++i) {
    EntityHelper::markIDForCleanup(live_id);
    EntityHelper::cleanup();
    REQUIRE_FALSE(EntityHelper::resolve(h).valid());

    Entity &e2 = EntityHelper::createEntity();
    live_id = e2.id;
    EntityHelper::merge_entity_arrays();
    EntityHandle h2 = EntityHelper::handle_for(e2);
    REQUIRE(h2.valid());

    // We should generally reuse the freed slot, but even if the allocator
    // changes in the future, the *safety* guarantee is what matters:
    // stale handle must never resolve, and new handle must resolve.
    REQUIRE(EntityHelper::resolve(h2).asE().id == e2.id);
    REQUIRE_FALSE(EntityHelper::resolve(h).valid());

    h = h2;
  }
}

TEST_CASE("EntityHandle - hard reset clears singleton + permanents",
          "[EntityHandle]") {
  reset_ecs_world_for_test();

  // Create a permanent entity and register it as a singleton.
  struct TestSingleton : BaseComponent {};

  Entity &perm = EntityHelper::createPermanentEntity();
  perm.addComponent<TestSingleton>();
  EntityHelper::merge_entity_arrays();
  EntityHelper::registerSingleton<TestSingleton>(perm);
  REQUIRE(EntityHelper::get_singleton<TestSingleton>().get().id == perm.id);

  // Hard reset should clear singleton map + permanent set.
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // If singletonMap wasn't cleared this would return the old pointer.
  // After reset, we expect to fall back to dummy entity.
  REQUIRE(EntityHelper::get_singleton<TestSingleton>().get().id != perm.id);
}

} // namespace afterhours

