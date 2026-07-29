// system_tags_test.cpp
// Regression test for #41: System tag filtering (tags::All / tags::Any /
// tags::None) was guarded behind `#if __APPLE__`, with a `tags_ok -> return
// true` stub on every other platform. That silently disabled tag filtering off
// macOS: a tagged System ran `for_each_with` against EVERY entity. This test
// drives a tagged System through SystemManager::tick and asserts only the
// correctly-tagged entities are visited, on every platform.
//
// Build (from tests/, via the Makefile):  make system_tags_test
// or standalone:
//   clang++ -std=c++20 -isystem <root-with-afterhours-symlink> -isystem vendor \
//     system_tags_test.cpp -o /tmp/t && /tmp/t

#include <afterhours/ah.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

static int tests_run = 0;
static int tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

enum class GTag : TagId { Enemy = 1, Boss = 2, Friendly = 3 };

struct Health : BaseComponent {
  int hp = 100;
};

// Collects every entity id it is invoked on, so we can assert exactly which
// entities passed the tag filter.
template <typename Filter> struct Collector : System<Health, Filter> {
  std::vector<int> visited;
  void for_each_with(Entity &e, Health &, const float) override {
    visited.push_back((int)e.id);
  }
};

static void reset_world() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// helper: make an entity with Health + the given tags
template <typename... Tags> static Entity &make(Tags... tags) {
  auto &e = EntityHelper::createEntity();
  e.addComponent<Health>();
  (e.enableTag(tags), ...);
  return e;
}

// tags::All<Enemy> visits ONLY entities tagged Enemy — not untagged ones.
void test_all_filters_untagged() {
  reset_world();
  int enemy = (int)make(GTag::Enemy).id;
  make(); // untagged
  make(); // untagged
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  auto sys = std::make_unique<Collector<tags::All<GTag::Enemy>>>();
  auto *raw = sys.get();
  sm.register_update_system(std::move(sys));
  sm.tick(EntityHelper::get_entities_for_mod(), 0.f);

  CHECK(raw->visited.size() == 1);
  CHECK(raw->visited.size() == 1 && raw->visited[0] == enemy);
}

// tags::All<Enemy, Boss> requires BOTH tags.
void test_all_requires_every_tag() {
  reset_world();
  make(GTag::Enemy);            // only one of the two -> excluded
  int boss = (int)make(GTag::Enemy, GTag::Boss).id; // both -> included
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  auto sys = std::make_unique<Collector<tags::All<GTag::Enemy, GTag::Boss>>>();
  auto *raw = sys.get();
  sm.register_update_system(std::move(sys));
  sm.tick(EntityHelper::get_entities_for_mod(), 0.f);

  CHECK(raw->visited.size() == 1);
  CHECK(raw->visited.size() == 1 && raw->visited[0] == boss);
}

// tags::None<Friendly> excludes anything tagged Friendly.
void test_none_excludes() {
  reset_world();
  int a = (int)make(GTag::Enemy).id;   // not friendly -> included
  make(GTag::Friendly);                // friendly -> excluded
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  auto sys = std::make_unique<Collector<tags::None<GTag::Friendly>>>();
  auto *raw = sys.get();
  sm.register_update_system(std::move(sys));
  sm.tick(EntityHelper::get_entities_for_mod(), 0.f);

  CHECK(raw->visited.size() == 1);
  CHECK(raw->visited.size() == 1 && raw->visited[0] == a);
}

// A System with NO tag requirement still visits everything (guard's happy path).
void test_no_requirement_visits_all() {
  reset_world();
  make();
  make(GTag::Enemy);
  make(GTag::Friendly);
  EntityHelper::merge_entity_arrays();

  SystemManager sm;
  struct Plain : System<Health> {
    int seen = 0;
    void for_each_with(Entity &, Health &, const float) override { seen++; }
  };
  auto sys = std::make_unique<Plain>();
  auto *raw = sys.get();
  sm.register_update_system(std::move(sys));
  sm.tick(EntityHelper::get_entities_for_mod(), 0.f);

  CHECK(raw->seen == 3);
}

int main() {
  printf("=== system tag-filtering tests (#41 regression) ===\n\n");
  struct T { const char *n; void (*f)(); };
  T tests[] = {
    {"all_filters_untagged", test_all_filters_untagged},
    {"all_requires_every_tag", test_all_requires_every_tag},
    {"none_excludes", test_none_excludes},
    {"no_requirement_visits_all", test_no_requirement_visits_all},
  };
  for (auto &t : tests) { printf("  Running: %s\n", t.n); t.f(); }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) { printf("FAILURES: %d\n", tests_run - tests_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
