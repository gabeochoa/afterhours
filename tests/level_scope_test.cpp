#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <afterhours/ah.h>
#include <afterhours/src/core/level_scope.h>
#include <cstdio>
#include <vector>
using namespace afterhours;
static std::vector<void (*)()> &tests() {
  static std::vector<void (*)()> t;
  return t;
}
struct Reg {
  Reg(void (*f)()) { tests().push_back(f); }
};
#define TEST(name)                                                             \
  static void name();                                                          \
  static Reg reg_##name(name);                                                 \
  static void name()
static int failures = 0;
#define CHECK(expr)                                                            \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ++failures;                                                              \
      fprintf(stderr, "  FAIL: %s (%s:%d)\n", #expr, __FILE__, __LINE__);      \
    }                                                                          \
  } while (0)
struct Marker : BaseComponent {
  int v = 0;
};
struct Fixture {
  EntityCollection coll;
  Fixture() {
    level_scope::clear_registrations();
    EntityHelper::set_default_collection(&coll);
  }

  ~Fixture() {
    EntityHelper::set_default_collection(nullptr);
    level_scope::clear_registrations();
  }

  Entity &create(int level) {
    auto guard = level_scope::scoped(level);
    return EntityHelper::createEntity();
  }
};
TEST(interleaved_unload_leaves_other_level) {
  Fixture f;
  Entity &a1 = f.create(1);
  Entity &b1 = f.create(2);
  Entity &a2 = f.create(1);
  Entity &b2 = f.create(2);
  Entity &g = EntityHelper::createEntity();
  const int a1_id = a1.id, a2_id = a2.id;
  const int b1_id = b1.id, b2_id = b2.id, g_id = g.id;
  CHECK(level_scope::count_in_level(1) == 2);
  CHECK(level_scope::count_in_level(2) == 2);
  CHECK(level_scope::count_in_level(0) == 1);
  level_scope::unload_level(1);
  CHECK(!EntityHelper::getEntityForID(a1_id).valid());
  CHECK(!EntityHelper::getEntityForID(a2_id).valid());
  CHECK(EntityHelper::getEntityForID(b1_id).valid());
  CHECK(EntityHelper::getEntityForID(b2_id).valid());
  CHECK(EntityHelper::getEntityForID(g_id).valid());
  CHECK(level_scope::count_in_level(2) == 2);
  CHECK(level_scope::count_in_level(1) == 0);
}

TEST(cache_clears_fire_per_level) {
  Fixture f;
  int cleared_a = 0, cleared_b = 0;
  level_scope::register_cache_clear(1, [&] { ++cleared_a; });
  level_scope::register_cache_clear(2, [&] { ++cleared_b; });
  (void)f.create(1);
  (void)f.create(2);
  level_scope::unload_level(1);
  CHECK(cleared_a == 1);
  CHECK(cleared_b == 0);
  level_scope::unload_level(1);
  CHECK(cleared_a == 1);
  level_scope::unload_level(2);
  CHECK(cleared_b == 1);
}

TEST(nested_guards_restore) {
  Fixture f;
  Entity *inner = nullptr;
  {
    auto outer = level_scope::scoped(1);
    {
      auto in = level_scope::scoped(2);
      Entity &e = EntityHelper::createEntity();
      inner = &e;
      CHECK(e.level_id == 2);
    }
    CHECK(level_scope::current_level() == 1);
    Entity &e = EntityHelper::createEntity();
    CHECK(e.level_id == 1);
  }
  CHECK(level_scope::current_level() == kUnscopedLevel);
  CHECK(inner->level_id == 2);
  CHECK(level_scope::count_in_level(2) == 1);
}

TEST(singleton_deregistered_on_unload) {
  Fixture f;
  Entity &e = f.create(1);
  e.addComponent<Marker>();
  EntityHelper::registerSingleton<Marker>(e);
  CHECK(EntityHelper::has_singleton<Marker>());
  level_scope::unload_level(1);
  CHECK(!EntityHelper::has_singleton<Marker>());
}

TEST(reset_all_levels_and_hook) {
  Fixture f;
  int resets = 0;
  level_scope::on_test_reset([&] { ++resets; });
  (void)f.create(1);
  (void)f.create(2);
  Entity &g = EntityHelper::createEntity();
  const int g_id = g.id;
  level_scope::reset_all_levels();
  CHECK(resets == 1);
  CHECK(level_scope::count_in_level(1) == 0);
  CHECK(level_scope::count_in_level(2) == 0);
  CHECK(EntityHelper::getEntityForID(g_id).valid());
  (void)f.create(1);
  level_scope::reset_all_levels();
  CHECK(resets == 2);
}

TEST(recycled_entity_not_stamped_stale) {
  Fixture f;
  EntityCollection &coll = EntityHelper::get_default_collection();
  coll.reserve_entities(1);
  {
    auto guard = level_scope::scoped(1);
    Entity &e = EntityHelper::createEntity();
    e.cleanup = true;
  }
  coll.cleanup();
  Entity &e2 = EntityHelper::createEntity();
  CHECK(e2.level_id == kUnscopedLevel);
  CHECK(level_scope::count_in_level(1) == 0);
}

int main() {
  for (auto fn : tests())
    fn();
  if (failures) {
    printf("FAILURES: %d\n", failures);
    return 1;
  }
  printf("ALL TESTS PASSED.\n");
  return 0;
}
