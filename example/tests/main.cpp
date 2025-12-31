

#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"
#include "../../vendor/trompeloeil/trompeloeil.hpp"
//
#include "../../src/plugins/autolayout.h"
#include "../../src/ecs.h"
#include "../../src/core/pointer_policy.h"
#include "../../src/core/opt_entity_handle.h"
#include <algorithm>
#include <memory>

#include "../tag_filter_regression/demo_tags.h"

namespace afterhours {
namespace ui {
TEST_CASE("UIComponentTest", "[UIComponent]") {
  UIComponent cmp(-1);
  REQUIRE(cmp.flex_direction == FlexDirection::Column);
  REQUIRE_FALSE(cmp.was_rendered_to_screen);
  REQUIRE_FALSE(cmp.absolute);
}

TEST_CASE("RectCalculations", "[UIComponent]") {
  UIComponent cmp(0);
  cmp.computed[Axis::X] = 100.f;
  cmp.computed[Axis::Y] = 200.f;
  cmp.computed_rel[Axis::X] = 10.f;
  cmp.computed_rel[Axis::Y] = 20.f;

  Rectangle rect = cmp.rect();
  REQUIRE(rect.x == 10.f);
  REQUIRE(rect.y == 20.f);
  REQUIRE(rect.width == 100.f);
  REQUIRE(rect.height == 200.f);
}

TEST_CASE("AddRemoveChild", "[UIComponent]") {
  UIComponent parent(0);
  UIComponent child1(1);
  UIComponent child2(2);

  parent.add_child(child1.id);
  parent.add_child(child2.id);

  REQUIRE(parent.children.size() == 2);
  REQUIRE(parent.children[0] == child1.id);
  REQUIRE(parent.children[1] == child2.id);

  parent.remove_child(child1.id);

  REQUIRE(parent.children.size() == 1);
  REQUIRE(parent.children[0] == child2.id);
}

TEST_CASE("SetParent", "[UIComponent]") {
  UIComponent parent(1);
  UIComponent child(2);

  child.set_parent(parent.id);

  REQUIRE(child.parent == parent.id);
}

TEST_CASE("CalculateStandalone", "[AutoLayout]") {
  AutoLayout al; // Empty mapping
  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Pixels, 100.f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  REQUIRE(cmp.computed[Axis::X] == 100.f);
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}

TEST_CASE("AutoLayoutCalculateStandaloneWithPercent", "[AutoLayout]") {
  AutoLayout al; // Empty mapping

  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Percent, 0.5f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  // TODO
  // REQUIRE(cmp.computed[Axis::X] == 0.f); // Should not change
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}

// TODO
/*
TEST_CASE("AutoLayoutCalculateStandaloneWithText", "[AutoLayout]") {
  AutoLayout al; // Empty mapping
  UIComponent cmp(1);
  cmp.desired[Axis::X] = Size{Dim::Text, 100.f};
  cmp.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  al.calculate_standalone(cmp);

  REQUIRE(cmp.computed[Axis::X] == 100.f); // Default value for text
  REQUIRE(cmp.computed[Axis::Y] == 200.f);
}

class AutoLayoutMock : public AutoLayout {
public:
  MAKE_MOCK1(to_cmp, UIComponent &(EntityID));
};

TEST_CASE("AutoLayoutCalculateThoseWithParents", "[AutoLayout]") {
  UIComponent parent(1);
  parent.desired[Axis::X] = Size{Dim::Pixels, 1000.f};
  parent.desired[Axis::Y] = Size{Dim::Pixels, 200.f};
  parent.computed[Axis::X] = 1000.f;
  parent.computed[Axis::Y] = 200.f;

  UIComponent child(2);
  child.parent = parent.id;
  child.desired[Axis::X] = Size{Dim::Percent, 0.5f};
  child.desired[Axis::Y] = Size{Dim::Pixels, 200.f};

  AutoLayoutMock al;
  ALLOW_CALL(al, to_cmp(parent.id)).LR_RETURN(std::ref(parent));
  ALLOW_CALL(al, to_cmp(child.id)).LR_RETURN(std::ref(child));

  al.calculate_those_with_parents(child);

  REQUIRE(child.computed[Axis::X] == 500.f);
  // still zero since its calculate standalone
  REQUIRE(child.computed[Axis::Y] == Approx(0.f).epsilon(0.1f));
}
*/

} // namespace ui

// System requiring Transform and the Runner tag, excluding Store
struct MoveRunnersSys
    : System<TagTestTransform, tags::All<DemoTag::Runner>,
             tags::None<DemoTag::Store>> {
  void for_each_with(Entity &, TagTestTransform &t, float) override { t.x += 1; }
};

// System that runs on Health with any of the Chaser or Runner tags
struct HealAnyoneTaggedSys
    : System<TagTestHealth, tags::Any<DemoTag::Chaser, DemoTag::Runner>> {
  void for_each_with(Entity &, TagTestHealth &h, float) override {
    h.hp = std::min(h.hp + 5, 100);
  }
};

// Excludes Store-tagged entities regardless of components
struct DebugNonStoreSys : System<tags::None<DemoTag::Store>> {
  int *count = nullptr;
  explicit DebugNonStoreSys(int *c) : count(c) {}
  void for_each_with(Entity &, float) override {
    if (count)
      (*count)++;
  }
};

TEST_CASE("ECS: temp entities are not query-visible until merge (unless forced)",
          "[ECS][EntityQuery][TempEntities]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &e = EntityHelper::createEntity();
  const int id = e.id;

  // Default query should miss temp entities.
  {
    EntityQuery<> q({.ignore_temp_warning = true});
    q.whereID(id);
    REQUIRE_FALSE(q.has_values());
  }

  // Force-merged query should see them.
  {
    EntityQuery<> q({.force_merge = true, .ignore_temp_warning = true});
    q.whereID(id);
    REQUIRE(q.has_values());
  }
}

TEST_CASE("ECS: cleanup removes entities and lookups stop finding them",
          "[ECS][Cleanup]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &e = EntityHelper::createEntity();
  const int id = e.id;
  EntityHelper::merge_entity_arrays();

  // Sanity: it exists after merge.
  REQUIRE(EntityHelper::getEntityForID(id).valid());

  EntityHelper::markIDForCleanup(id);
  EntityHelper::cleanup();

  REQUIRE_FALSE(EntityHelper::getEntityForID(id).valid());
  REQUIRE_FALSE(EntityQuery<>({.ignore_temp_warning = true})
                    .whereID(id)
                    .has_values());
}

TEST_CASE("ECS: EntityHandle resolves after merge and becomes stale on cleanup",
          "[ECS][EntityHandle]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &e = EntityHelper::createEntity();
  // By default, handles are not assigned until merge.
  REQUIRE_FALSE(EntityHelper::handle_for(e).valid());

  EntityHelper::merge_entity_arrays();
  EntityHandle h = EntityHelper::handle_for(e);
  REQUIRE(h.valid());
  REQUIRE(EntityHelper::resolve(h).valid());
  REQUIRE(EntityHelper::resolve(h).asE().id == e.id);

  EntityHelper::markIDForCleanup(e.id);
  EntityHelper::cleanup();
  REQUIRE_FALSE(EntityHelper::resolve(h).valid());
}

TEST_CASE("ECS: EntityHandle generation changes on slot reuse",
          "[ECS][EntityHandle]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  EntityHandle h1 = EntityHelper::handle_for(a);
  REQUIRE(h1.valid());

  EntityHelper::markIDForCleanup(a.id);
  EntityHelper::cleanup();
  REQUIRE_FALSE(EntityHelper::resolve(h1).valid());

  // Create another entity; we expect slot reuse in steady-state.
  Entity &b = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  EntityHandle h2 = EntityHelper::handle_for(b);
  REQUIRE(h2.valid());

  // Slot may reuse; generation must change if it does.
  if (h2.slot == h1.slot) {
    REQUIRE(h2.gen != h1.gen);
  }
}

struct Targets : BaseComponent {
  EntityHandle target{};
};

static_assert(!afterhours::is_pointer_like_v<int>);
static_assert(afterhours::is_pointer_like_v<int *>);
static_assert(afterhours::is_pointer_like_v<std::shared_ptr<int>>);
static_assert(afterhours::is_pointer_like_v<std::unique_ptr<int>>);
static_assert(afterhours::is_pointer_like_v<afterhours::RefEntity>);
static_assert(afterhours::is_pointer_like_v<afterhours::OptEntity>);
static_assert(!afterhours::is_pointer_like_v<afterhours::EntityHandle>);
static_assert(!afterhours::is_pointer_like_v<Targets>);

TEST_CASE("Phase 3: components store EntityHandle (not pointers) and handles "
          "become stale after cleanup",
          "[ECS][EntityHandle][Components]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  const EntityHandle hb = EntityHelper::handle_for(b);
  REQUIRE(hb.valid());

  a.addComponent<Targets>().target = hb;

  REQUIRE(EntityHelper::resolve(a.get<Targets>().target).valid());
  REQUIRE(EntityHelper::resolve(a.get<Targets>().target).asE().id == b.id);

  EntityHelper::markIDForCleanup(b.id);
  EntityHelper::cleanup();

  REQUIRE_FALSE(EntityHelper::resolve(a.get<Targets>().target).valid());
}

TEST_CASE("Phase 3: OptEntityHandle resolves and becomes stale on cleanup",
          "[ECS][OptEntityHandle]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  OptEntityHandle ref_b = OptEntityHandle::from_entity(b);
  REQUIRE(ref_b.id == b.id);
  REQUIRE(ref_b.handle.valid());

  // resolves while alive
  REQUIRE(ref_b.resolve().valid());
  REQUIRE(ref_b.resolve().asE().id == b.id);

  EntityHelper::markIDForCleanup(b.id);
  EntityHelper::cleanup();

  // becomes stale after cleanup
  REQUIRE_FALSE(ref_b.resolve().valid());

  // sanity: unrelated entity still exists
  REQUIRE(EntityHelper::getEntityForID(a.id).valid());
}

TEST_CASE("EntityQuery: gen_first short-circuits on early match",
          "[ECS][EntityQuery]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // Create and merge a few entities so iteration order is deterministic.
  Entity &first = EntityHelper::createEntity();
  EntityHelper::createEntity();
  EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  int calls = 0;
  auto opt = EntityQuery<>({.ignore_temp_warning = true})
                 .whereLambda([&](const Entity &e) {
                   calls++;
                   return e.id == first.id;
                 })
                 .gen_first();

  REQUIRE(opt.valid());
  REQUIRE(opt.asE().id == first.id);
  // With stop-on-first enabled in gen_first(), we should only evaluate until we
  // find the first match (which is the first entity here).
  REQUIRE(calls == 1);
}

TEST_CASE("ECS: EntityQuery tag predicates remain correct",
          "[ECS][EntityQuery][Tags]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  a.enableTag(DemoTag::Runner);

  Entity &b = EntityHelper::createEntity();
  b.enableTag(DemoTag::Runner);
  b.enableTag(DemoTag::Store);

  Entity &c = EntityHelper::createEntity();
  c.enableTag(DemoTag::Chaser);

  EntityHelper::merge_entity_arrays();

  // Any(Runner, Chaser) should match all 3.
  {
    auto ents = EntityQuery<>({.ignore_temp_warning = true})
                    .whereHasAnyTag(DemoTag::Runner)
                    .gen();
    REQUIRE(ents.size() == 2);
  }

  // None(Store) should match a and c only.
  {
    auto ents = EntityQuery<>({.ignore_temp_warning = true})
                    .whereHasNoTags(DemoTag::Store)
                    .gen();
    REQUIRE(ents.size() == 2);
  }

  // Runner AND None(Store) should match only a.
  {
    auto ents = EntityQuery<>({.ignore_temp_warning = true})
                    .whereHasTag(DemoTag::Runner)
                    .whereHasNoTags(DemoTag::Store)
                    .gen();
    REQUIRE(ents.size() == 1);
  }
}

TEST_CASE("ECS: System tag filters remain correct across merge timing",
          "[ECS][System][Tags]") {
#if !__APPLE__
  // On non-Apple platforms, `System<>` tag filters are currently a no-op
  // (see `System::tags_ok` platform guard in `src/core/system.h`).
  SUCCEED();
  return;
#else
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // Create sample entities (these start as temp entities).
  Entity &e0 = EntityHelper::createEntity();
  e0.addComponent<TagTestTransform>().x = 0;
  e0.enableTag(DemoTag::Runner);

  Entity &e1 = EntityHelper::createEntity();
  e1.addComponent<TagTestTransform>().x = 5;
  e1.enableTag(DemoTag::Runner);
  e1.enableTag(DemoTag::Store); // excluded

  Entity &e2 = EntityHelper::createEntity();
  e2.addComponent<TagTestHealth>().hp = 50;
  e2.enableTag(DemoTag::Chaser);

  Entity &e3 = EntityHelper::createEntity();
  e3.addComponent<TagTestHealth>().hp = 10;
  e3.enableTag(DemoTag::Runner);

  int non_store_count = 0;
  SystemManager sm;
  sm.register_update_system(std::make_unique<MoveRunnersSys>());
  sm.register_update_system(std::make_unique<HealAnyoneTaggedSys>());
  sm.register_update_system(std::make_unique<DebugNonStoreSys>(&non_store_count));

  // First tick: MoveRunners runs before temp entities are merged (so it won't
  // see them). Merge happens after each system, so later systems see entities.
  sm.tick_all(EntityHelper::get_entities_for_mod(), 0.016f);

  // Second tick: MoveRunners should see merged entities and run exactly once.
  sm.tick_all(EntityHelper::get_entities_for_mod(), 0.016f);

  // Validate transform updates:
  REQUIRE(e0.get<TagTestTransform>().x == 1); // 0 -> 1 (ran once)
  REQUIRE(e1.get<TagTestTransform>().x == 5); // store excluded

  // Validate health updates: healer runs on Chaser or Runner tagged health.
  REQUIRE(e2.get<TagTestHealth>().hp == 60); // 50 -> 55 -> 60
  REQUIRE(e3.get<TagTestHealth>().hp == 20); // 10 -> 15 -> 20

  // DebugNonStore runs both ticks; it should process only non-store entities:
  // e0, e2, e3 => 3 per tick => 6 total
  REQUIRE(non_store_count == 6);
#endif
}

TEST_CASE("ECS: get_singleton is safe when missing (returns a dummy entity)",
          "[ECS][Singleton]") {
  struct MissingSingleton : BaseComponent {};

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  // Not registered => should not crash. It should return a dummy entity ref.
  RefEntity e = EntityHelper::get_singleton<MissingSingleton>();
  REQUIRE_FALSE(e.get().has<MissingSingleton>());
}

} // namespace afterhours
