// slot_at_creation_test.cpp
//
// A slot is assigned when the entity is created, not when it is merged, so
// handle_for() works on an entity still sitting in temp.
//
// Merge grants two unrelated things: identity (a slot, which is what
// handle_for needs) and visibility (membership of the main array, which is what
// queries walk). Callers wanting only the first used to trigger both mid-frame.
// Only identity moved -- temp is still invisible to queries, and the last test
// here pins that so the split does not quietly widen.
//
// Build (from afterhours/tests/):
//   make slot_at_creation_test
// Run:
//   ./slot_at_creation_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                                                             \
  static void test_##name();                                                   \
  struct Register_##name {                                                     \
    Register_##name() { register_test(#name, test_##name); }                   \
  } register_##name##_instance;                                                \
  static void test_##name()

struct TestEntry {
  const char *name;
  void (*fn)();
};

static std::vector<TestEntry> &test_registry() {
  static std::vector<TestEntry> r;
  return r;
}

static void register_test(const char *name, void (*fn)()) {
  test_registry().push_back({name, fn});
}

static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) {
    tests_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

struct Marker : BaseComponent {
  int value;
  Marker() : value(0) {}
  Marker(int v) : value(v) {}
};

static void clear_all() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// ============================================================================
// Identity before the merge
// ============================================================================

TEST(unmerged_entity_has_a_resolvable_handle) {
  clear_all();
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Marker>(7);

  // No merge_entity_arrays() call anywhere in this test.
  EntityHandle h = EntityHelper::handle_for(e);
  CHECK(h.is_valid());

  OptEntity got = EntityHelper::resolve(h);
  CHECK(got.has_value());
  CHECK(got->id == e.id);
  CHECK(got->get<Marker>().value == 7);

  clear_all();
}

TEST(the_handle_is_unchanged_by_the_merge) {
  clear_all();
  Entity &e = EntityHelper::createEntity();
  const EntityHandle before = EntityHelper::handle_for(e);
  CHECK(before.is_valid());

  EntityHelper::merge_entity_arrays();

  const EntityHandle after = EntityHelper::handle_for(e);
  // Same slot and generation: a handle stored pre-merge stays good.
  CHECK(before == after);
  CHECK(EntityHelper::resolve(before).has_value());

  clear_all();
}

TEST(a_parent_handle_taken_pre_merge_works_for_children) {
  clear_all();
  // The shape entity_makers.cpp uses: build a parent, hand its handle to the
  // children it creates, all before any merge.
  Entity &parent = EntityHelper::createEntity();
  const EntityHandle parent_h = EntityHelper::handle_for(parent);
  CHECK(parent_h.is_valid());

  std::vector<EntityHandle> children;
  for (int i = 0; i < 3; i++) {
    Entity &c = EntityHelper::createEntity();
    c.addComponent<Marker>(i);
    children.push_back(EntityHelper::handle_for(c));
  }

  for (int i = 0; i < 3; i++) {
    CHECK(children[i].is_valid());
    CHECK(children[i] != parent_h);
    OptEntity c = EntityHelper::resolve(children[i]);
    CHECK(c.has_value());
    CHECK(c->get<Marker>().value == i);
  }

  EntityHelper::merge_entity_arrays();
  CHECK(EntityHelper::resolve(parent_h).has_value());
  for (const EntityHandle &h : children)
    CHECK(EntityHelper::resolve(h).has_value());

  clear_all();
}

TEST(distinct_entities_get_distinct_slots_before_merge) {
  clear_all();
  Entity &a = EntityHelper::createEntity();
  Entity &b = EntityHelper::createEntity();

  const EntityHandle ha = EntityHelper::handle_for(a);
  const EntityHandle hb = EntityHelper::handle_for(b);
  CHECK(ha.is_valid() && hb.is_valid());
  CHECK(ha != hb);
  CHECK(ha.slot != hb.slot);

  clear_all();
}

// ============================================================================
// The slot must not leak when the entity dies before its first merge
// ============================================================================

TEST(cleanup_before_merge_releases_the_slot) {
  clear_all();
  Entity &doomed = EntityHelper::createEntity();
  const EntityHandle stale = EntityHelper::handle_for(doomed);
  CHECK(stale.is_valid());
  const EntityHandle::Slot released = stale.slot;

  doomed.cleanup = true;
  // merge drops a cleanup-marked temp entity outright; it never reaches
  // cleanup()'s sweep, so merge is the only place that can free its slot.
  EntityHelper::merge_entity_arrays();

  CHECK(!EntityHelper::resolve(stale).has_value());

  // The freed slot is reused rather than stranded.
  Entity &next = EntityHelper::createEntity();
  const EntityHandle reused = EntityHelper::handle_for(next);
  CHECK(reused.is_valid());
  CHECK(reused.slot == released);
  // Same slot, bumped generation, so the old handle stays dead.
  CHECK(reused != stale);
  CHECK(!EntityHelper::resolve(stale).has_value());
  CHECK(EntityHelper::resolve(reused).has_value());

  clear_all();
}

TEST(repeated_create_and_cleanup_does_not_grow_the_slot_table) {
  clear_all();
  // Churn well past any plausible table growth; if slots leaked, each pass
  // would strand one and the slot values would climb without bound.
  EntityHandle::Slot max_slot = 0;
  for (int i = 0; i < 200; i++) {
    Entity &e = EntityHelper::createEntity();
    const EntityHandle h = EntityHelper::handle_for(e);
    CHECK(h.is_valid());
    if (h.slot > max_slot)
      max_slot = h.slot;
    e.cleanup = true;
    EntityHelper::merge_entity_arrays();
  }
  CHECK(max_slot < 8);

  clear_all();
}

TEST(teardown_with_entities_in_both_arrays_leaves_nothing_resolvable) {
  clear_all();
  Entity &merged = EntityHelper::createEntity();
  const EntityHandle merged_h = EntityHelper::handle_for(merged);
  EntityHelper::merge_entity_arrays();

  Entity &still_temp = EntityHelper::createEntity();
  const EntityHandle temp_h = EntityHelper::handle_for(still_temp);

  CHECK(EntityHelper::resolve(merged_h).has_value());
  CHECK(EntityHelper::resolve(temp_h).has_value());

  clear_all();

  CHECK(!EntityHelper::resolve(merged_h).has_value());
  CHECK(!EntityHelper::resolve(temp_h).has_value());
}

// ============================================================================
// Visibility deliberately did NOT move
// ============================================================================

TEST(a_temp_entity_is_still_invisible_to_a_plain_query) {
  clear_all();
  Entity &e = EntityHelper::createEntity();
  e.addComponent<Marker>(1);

  // Only identity moved to creation time. If this ever starts returning 1, the
  // split has widened into a query-semantics change and every consumer of the
  // library is affected -- see the plan's Finding 4.
  CHECK(EntityQuery<>({.ignore_temp_warning = true})
            .whereHasComponent<Marker>()
            .gen_count() == 0);

  EntityHelper::merge_entity_arrays();
  CHECK(EntityQuery<>().whereHasComponent<Marker>().gen_count() == 1);

  clear_all();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== slot at creation tests ===\n\n");

  for (auto &entry : test_registry()) {
    printf("  Running: %s\n", entry.name);
    entry.fn();
  }

  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed == tests_run) {
    printf("All checks passed!\n");
    return 0;
  }
  printf("SOME TESTS FAILED!\n");
  return 1;
}
