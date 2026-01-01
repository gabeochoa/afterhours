
#define CATCH_CONFIG_MAIN
#include "../../vendor/catch2/catch.hpp"

#include "../../src/ecs.h"

namespace afterhours {

struct SnapshotTestCmp : BaseComponent {
  int x = 0;
};

struct SnapshotTestValue {
  int x = 0;
};

static_assert(!afterhours::is_pointer_like_v<SnapshotTestValue>);

TEST_CASE("ECS: snapshot_for<T>(projector) returns (EntityHandle, value) pairs",
          "[ECS][Snapshot]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  EntityHelper::createEntity(); // entity without component
  Entity &c = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  a.addComponent<SnapshotTestCmp>().x = 10;
  c.addComponent<SnapshotTestCmp>().x = 30;

  const auto snap = snapshot_for<SnapshotTestCmp>([](const SnapshotTestCmp &cmp) {
    return SnapshotTestValue{.x = cmp.x};
  });

  REQUIRE(snap.size() == 2);

  for (const auto &[h, v] : snap) {
    REQUIRE(h.valid());
    auto resolved = EntityHelper::resolve(h);
    REQUIRE(resolved.valid());
    const int id = resolved.asE().id;
    if (id == a.id) {
      REQUIRE(v.x == 10);
    } else if (id == c.id) {
      REQUIRE(v.x == 30);
    } else {
      FAIL("snapshot contains unexpected entity");
    }
  }
}

TEST_CASE("ECS: apply_snapshot can restore projected values onto entities",
          "[ECS][Snapshot][Apply]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();

  a.addComponent<SnapshotTestCmp>().x = 1;
  b.addComponent<SnapshotTestCmp>().x = 2;

  const auto snap = snapshot_for<SnapshotTestCmp>([](const SnapshotTestCmp &cmp) {
    return SnapshotTestValue{.x = cmp.x};
  });
  REQUIRE(snap.size() == 2);

  // Mutate state away from snapshot.
  a.get<SnapshotTestCmp>().x = 0;
  b.get<SnapshotTestCmp>().x = 0;

  const auto res = apply_snapshot<SnapshotTestValue>(
      snap,
      [](Entity &e, const SnapshotTestValue &v) {
        // Ensure component exists and apply.
        if (!e.has<SnapshotTestCmp>()) {
          e.addComponent<SnapshotTestCmp>();
        }
        e.get<SnapshotTestCmp>().x = v.x;
      });

  REQUIRE(res.applied == 2);
  REQUIRE(res.skipped_invalid_handle == 0);
  REQUIRE(res.skipped_unresolved == 0);
  REQUIRE(res.spawned == 0);

  REQUIRE(a.get<SnapshotTestCmp>().x == 1);
  REQUIRE(b.get<SnapshotTestCmp>().x == 2);
}

TEST_CASE("ECS: apply_snapshot MissingEntityPolicy::Error reports stale handles",
          "[ECS][Snapshot][Apply]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  a.addComponent<SnapshotTestCmp>().x = 42;

  const auto snap = snapshot_for<SnapshotTestCmp>([](const SnapshotTestCmp &cmp) {
    return SnapshotTestValue{.x = cmp.x};
  });
  REQUIRE(snap.size() == 1);

  const EntityHandle old_handle = snap[0].first;
  REQUIRE(old_handle.valid());

  // Delete the entity so the handle becomes stale/unresolvable.
  EntityHelper::markIDForCleanup(a.id);
  EntityHelper::cleanup();

  ApplySnapshotOptions opts{};
  opts.missing_entity_policy = ApplySnapshotOptions::MissingEntityPolicy::Error;

  const auto res = apply_snapshot<SnapshotTestValue>(
      snap,
      [](Entity &, const SnapshotTestValue &) {
        FAIL("apply should not run when MissingEntityPolicy::Error is used");
      },
      opts);

  REQUIRE(res.applied == 0);
  REQUIRE(res.spawned == 0);
  REQUIRE(res.skipped_unresolved == 0);
  REQUIRE(res.errors == 1);
  REQUIRE(res.first_error.slot == old_handle.slot);
  REQUIRE(res.first_error.gen == old_handle.gen);
}

TEST_CASE("ECS: apply_snapshot MissingEntityPolicy::Skip counts skipped_unresolved",
          "[ECS][Snapshot][Apply]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  a.addComponent<SnapshotTestCmp>().x = 7;

  const auto snap = snapshot_for<SnapshotTestCmp>([](const SnapshotTestCmp &cmp) {
    return SnapshotTestValue{.x = cmp.x};
  });
  REQUIRE(snap.size() == 1);

  EntityHelper::markIDForCleanup(a.id);
  EntityHelper::cleanup();

  ApplySnapshotOptions opts{};
  opts.missing_entity_policy = ApplySnapshotOptions::MissingEntityPolicy::Skip;

  const auto res = apply_snapshot<SnapshotTestValue>(
      snap, [](Entity &, const SnapshotTestValue &) { FAIL("apply must not run"); },
      opts);

  REQUIRE(res.applied == 0);
  REQUIRE(res.spawned == 0);
  REQUIRE(res.errors == 0);
  REQUIRE(res.skipped_unresolved == 1);
}

TEST_CASE("ECS: apply_snapshot MissingEntityPolicy::Create can spawn and apply",
          "[ECS][Snapshot][Apply]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  Entity &a = EntityHelper::createEntity();
  EntityHelper::merge_entity_arrays();
  a.addComponent<SnapshotTestCmp>().x = 99;

  const auto snap = snapshot_for<SnapshotTestCmp>([](const SnapshotTestCmp &cmp) {
    return SnapshotTestValue{.x = cmp.x};
  });
  REQUIRE(snap.size() == 1);

  EntityHelper::markIDForCleanup(a.id);
  EntityHelper::cleanup();

  ApplySnapshotOptions opts{};
  opts.missing_entity_policy = ApplySnapshotOptions::MissingEntityPolicy::Create;

  const auto res = apply_snapshot<SnapshotTestValue>(
      snap,
      [](Entity &e, const SnapshotTestValue &v) {
        if (!e.has<SnapshotTestCmp>())
          e.addComponent<SnapshotTestCmp>();
        e.get<SnapshotTestCmp>().x = v.x;
      },
      []() -> Entity & { return EntityHelper::createEntity(); }, opts);

  REQUIRE(res.applied == 0);
  REQUIRE(res.errors == 0);
  REQUIRE(res.skipped_unresolved == 0);
  REQUIRE(res.spawned == 1);

  // The created entity was merged by apply_snapshot (merge_new_entities default).
  // Verify at least one entity now has the restored component value.
  bool found = false;
  for (auto &sp : EntityHelper::get_entities()) {
    if (!sp)
      continue;
    if (sp->has<SnapshotTestCmp>() && sp->get<SnapshotTestCmp>().x == 99) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("ECS: apply_snapshot skips invalid handles when configured",
          "[ECS][Snapshot][Apply]") {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();

  std::vector<std::pair<EntityHandle, SnapshotTestValue>> snap;
  snap.emplace_back(EntityHandle::invalid(), SnapshotTestValue{.x = 1});

  ApplySnapshotOptions opts{};
  opts.skip_invalid_handles = true;
  opts.missing_entity_policy = ApplySnapshotOptions::MissingEntityPolicy::Error;

  const auto res =
      apply_snapshot<SnapshotTestValue>(snap,
                                       [](Entity &, const SnapshotTestValue &) {
                                         FAIL("apply must not run for invalid handle");
                                       },
                                       opts);

  REQUIRE(res.skipped_invalid_handle == 1);
  REQUIRE(res.errors == 0);
  REQUIRE(res.applied == 0);
  REQUIRE(res.spawned == 0);
}

} // namespace afterhours

