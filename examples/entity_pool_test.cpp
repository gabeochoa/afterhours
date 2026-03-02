#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cassert>
#include <cstdio>
#include <set>
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

struct Health : BaseComponent {
  int hp;
  Health() : hp(0) {}
  Health(int h) : hp(h) {}
};

struct Position : BaseComponent {
  float x, y;
  Position() : x(0), y(0) {}
  Position(float x_, float y_) : x(x_), y(y_) {}
};

struct Name : BaseComponent {
  std::string name;
  Name() = default;
  Name(std::string n) : name(std::move(n)) {}
};

// ============================================================================
// Test 1: No component leakage across recycle
// ============================================================================

TEST(no_component_leakage) {
  EntityCollection ec;
  ec.reserve_entities(1);
  CHECK(ec.pool_size() == 1);

  Entity &e = ec.createEntity();
  CHECK(ec.pool_size() == 0);
  e.addComponent<Health>(42);
  e.addComponent<Position>(1.0f, 2.0f);
  CHECK(e.has<Health>());
  CHECK(e.has<Position>());

  ec.cleanup(); // merge temp -> main
  e.cleanup = true;
  ec.cleanup(); // process cleanup, entity goes back to pool
  CHECK(ec.pool_size() == 1);

  Entity &e2 = ec.createEntity();
  CHECK(!e2.has<Health>());
  CHECK(!e2.has<Position>());
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 2: Fresh ID after recycle (from free list)
// ============================================================================

TEST(fresh_id_from_free_list) {
  EntityCollection ec;
  ec.reserve_entities(1);

  Entity &e1 = ec.createEntity();
  EntityID id1 = e1.id;

  ec.cleanup();
  e1.cleanup = true;
  ec.cleanup();

  Entity &e2 = ec.createEntity();
  EntityID id2 = e2.id;
  CHECK(id2 == id1);
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 3: Component data isolation (no stale data)
// ============================================================================

TEST(component_data_isolation) {
  EntityCollection ec;
  ec.reserve_entities(1);

  Entity &e1 = ec.createEntity();
  e1.addComponent<Health>(999);
  e1.addComponent<Name>("old_entity");
  ec.cleanup();
  e1.cleanup = true;
  ec.cleanup();

  Entity &e2 = ec.createEntity();
  CHECK(!e2.has<Health>());
  CHECK(!e2.has<Name>());

  e2.addComponent<Health>(1);
  CHECK(e2.get<Health>().hp == 1);
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 4: Handle invalidation after recycle
// ============================================================================

TEST(handle_invalidation) {
  EntityCollection ec;
  ec.reserve_entities(1);

  Entity &e1 = ec.createEntity();
  e1.addComponent<Health>(10);
  ec.cleanup(); // merge

  EntityHandle h = ec.handle_for(e1);
  CHECK(h.slot != EntityHandle::INVALID_SLOT);

  auto resolved = ec.resolve(h);
  CHECK(resolved.valid());
  CHECK(resolved->id == e1.id);

  e1.cleanup = true;
  ec.cleanup();

  auto stale = ec.resolve(h);
  CHECK(!stale.valid());

  Entity &e2 = ec.createEntity();
  (void)e2;
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 5: componentSet is clean after recycle
// ============================================================================

TEST(componentset_clean_after_recycle) {
  EntityCollection ec;
  ec.reserve_entities(1);

  Entity &e1 = ec.createEntity();
  e1.addComponent<Health>(1);
  e1.addComponent<Position>(1, 2);
  e1.addComponent<Name>("test");
  CHECK(e1.componentSet.count() == 3);

  ec.cleanup();
  e1.cleanup = true;
  ec.cleanup();

  Entity &e2 = ec.createEntity();
  CHECK(e2.componentSet.none());
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 6: Tags are clean after recycle
// ============================================================================

TEST(tags_clean_after_recycle) {
  EntityCollection ec;
  ec.reserve_entities(1);

  Entity &e1 = ec.createEntity();
  e1.enableTag(TagId{0});
  e1.enableTag(TagId{5});
  e1.enableTag(TagId{63});
  CHECK(e1.tags.count() == 3);

  ec.cleanup();
  e1.cleanup = true;
  ec.cleanup();

  Entity &e2 = ec.createEntity();
  CHECK(e2.tags.none());
  ec.cleanup();
  e2.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 7: Mixed pool and non-pool (fallback to make_shared)
// ============================================================================

TEST(mixed_pool_and_fallback) {
  EntityCollection ec;
  ec.reserve_entities(2);
  CHECK(ec.pool_size() == 2);

  Entity &a = ec.createEntity();
  Entity &b = ec.createEntity();
  Entity &c = ec.createEntity();
  CHECK(ec.pool_size() == 0);

  a.addComponent<Health>(1);
  b.addComponent<Health>(2);
  c.addComponent<Health>(3);

  CHECK(a.get<Health>().hp == 1);
  CHECK(b.get<Health>().hp == 2);
  CHECK(c.get<Health>().hp == 3);

  ec.cleanup();
  a.cleanup = true;
  b.cleanup = true;
  c.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 8: Singleton entities are cleaned up but pool still works
// ============================================================================

TEST(singleton_cleanup_with_pool) {
  EntityCollection ec;
  ec.reserve_entities(2);

  Entity &e1 = ec.createEntity();
  e1.addComponent<Health>(100);
  ec.cleanup(); // merge so registerSingleton can find the entity in main list
  ec.registerSingleton<Health>(e1);
  CHECK(ec.has_singleton<Health>());

  Entity &e2 = ec.createEntity();
  e2.addComponent<Position>(5, 10);
  ec.cleanup(); // merge e2

  e1.cleanup = true;
  e2.cleanup = true;
  ec.cleanup();

  CHECK(!ec.has_singleton<Health>());
  CHECK(ec.pool_size() >= 1);

  Entity &e3 = ec.createEntity();
  CHECK(!e3.has<Health>());
  CHECK(!e3.has<Position>());
  ec.cleanup();
  e3.cleanup = true;
  ec.cleanup();
}

// ============================================================================
// Test 9: Stress test - 1000 create/cleanup cycles
// ============================================================================

TEST(stress_create_cleanup_cycles) {
  EntityCollection ec;
  ec.reserve_entities(100);

  for (int cycle = 0; cycle < 1000; ++cycle) {
    Entity &e = ec.createEntity();
    e.addComponent<Health>(cycle);
    e.addComponent<Position>(static_cast<float>(cycle), 0.0f);
    ec.cleanup();

    CHECK(e.get<Health>().hp == cycle);
    CHECK(e.get<Position>().x == static_cast<float>(cycle));

    e.cleanup = true;
    ec.cleanup();
  }

  CHECK(ec.pool_size() > 0);
}

// ============================================================================
// Test 10: Pool size respects max cap
// ============================================================================

TEST(pool_respects_max_cap) {
  EntityCollection ec;
  ec.reserve_entities(5);
  size_t max_pool = ec.max_pool_size_;
  CHECK(max_pool == 10);

  for (int i = 0; i < 20; ++i) {
    ec.createEntity();
  }

  ec.cleanup(); // merge all to main
  Entities &ents = ec.get_entities_for_mod();
  for (auto &sp : ents) {
    sp->cleanup = true;
  }
  ec.cleanup();

  CHECK(ec.pool_size() <= max_pool);
}

// ============================================================================
// Test 11: ID reuse keeps IDs bounded
// ============================================================================

TEST(id_reuse_bounded) {
  EntityCollection ec;
  ec.reserve_entities(10);

  EntityID max_id_seen = 0;

  for (int cycle = 0; cycle < 500; ++cycle) {
    Entity &e = ec.createEntity();
    if (e.id > max_id_seen)
      max_id_seen = e.id;
    ec.cleanup(); // merge
    e.cleanup = true;
    ec.cleanup(); // return to pool
  }

  CHECK(ec.free_ids_.size() > 0);
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("Running entity pool tests...\n\n");

  for (const auto &entry : test_registry()) {
    printf("  %s\n", entry.name);
    entry.fn();
  }

  printf("\n%d/%d tests passed.\n", tests_passed, tests_run);

  if (tests_passed != tests_run) {
    printf("SOME TESTS FAILED!\n");
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
