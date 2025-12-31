
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

} // namespace afterhours

