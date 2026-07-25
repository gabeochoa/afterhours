// random_engine_test.cpp
// Coverage for the seedable RandomEngine (src/random_engine.h) and the
// ECS-integrated random path (EntityQuery::gen_random). The whole point of a
// seedable RNG is reproducibility: same seed -> same sequence. This locks that
// contract, plus the range/index bounds and the string-seed hashing.
//
// Build (from tests/, via the Makefile):  make random_engine_test
// Standalone (note the -DAFTER_HOURS_ENABLE_RANDOM build flag — random.h's own
// ODR guidance prefers the flag over an in-source define):
//   clang++ -std=c++20 -DAFTER_HOURS_ENABLE_RANDOM \
//     -isystem <root-with-afterhours-symlink> -isystem vendor \
//     random_engine_test.cpp -o /tmp/t && /tmp/t

#define AFTER_HOURS_ENTITY_HELPER
#define AFTER_HOURS_ENTITY_QUERY
#define AFTER_HOURS_SYSTEM
#include <afterhours/ah.h>
#include <afterhours/src/random_engine.h>

#include <cstdio>
#include <vector>

using namespace afterhours;

static int tests_run = 0, tests_passed = 0;
static void check(bool cond, const char *expr, const char *file, int line) {
  tests_run++;
  if (cond) tests_passed++;
  else fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

// Same seed -> identical sequence (the core reproducibility contract).
void test_same_seed_same_sequence() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{12345});
  std::vector<int> a;
  for (int i = 0; i < 16; i++) a.push_back(r.get_int(0, 1000000));
  r.set_seed(std::uint32_t{12345});
  std::vector<int> b;
  for (int i = 0; i < 16; i++) b.push_back(r.get_int(0, 1000000));
  CHECK(a == b);
}

// Different seeds -> (almost certainly) different sequences.
void test_different_seed_differs() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{1});
  std::vector<int> a;
  for (int i = 0; i < 16; i++) a.push_back(r.get_int(0, 1000000));
  r.set_seed(std::uint32_t{2});
  std::vector<int> b;
  for (int i = 0; i < 16; i++) b.push_back(r.get_int(0, 1000000));
  CHECK(a != b);
}

// String seed is stable: same string -> same sequence.
void test_string_seed_stable() {
  auto &r = RandomEngine::get();
  r.set_seed(std::string{"level-3"});
  int x = r.get_int(0, 1000000);
  r.set_seed(std::string{"level-3"});
  int y = r.get_int(0, 1000000);
  CHECK(x == y);
  CHECK(r.get_seed() == static_cast<std::uint32_t>(
                            std::hash<std::string>{}(std::string{"level-3"})));
}

// get_int is inclusive [a,b] and stays in range.
void test_int_range_bounds() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{7});
  bool in_range = true;
  for (int i = 0; i < 1000; i++) {
    int v = r.get_int(5, 10);
    if (v < 5 || v > 10) { in_range = false; break; }
  }
  CHECK(in_range);
  // degenerate range [k,k] always yields k
  CHECK(r.get_int(42, 42) == 42);
}

// get_float stays within [a,b).
void test_float_range_bounds() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{9});
  bool in_range = true;
  for (int i = 0; i < 1000; i++) {
    float v = r.get_float(-1.0f, 1.0f);
    if (v < -1.0f || v > 1.0f) { in_range = false; break; }
  }
  CHECK(in_range);
}

// get_index: valid index for non-empty, -1 for empty.
void test_get_index() {
  auto &r = RandomEngine::get();
  r.set_seed(std::uint32_t{3});
  std::vector<int> empty;
  CHECK(r.get_index(empty) == -1);
  std::vector<int> five(5, 0);
  bool valid = true;
  for (int i = 0; i < 1000; i++) {
    int idx = r.get_index(five);
    if (idx < 0 || idx >= 5) { valid = false; break; }
  }
  CHECK(valid);
}

// ECS-integrated gen_random: seeded -> reproducible entity pick; empty -> none.
void test_gen_random_reproducible() {
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  struct Mark : BaseComponent {};
  for (int i = 0; i < 10; i++)
    EntityHelper::createEntity().addComponent<Mark>();
  EntityHelper::merge_entity_arrays();

  RandomEngine::get().set_seed(std::uint32_t{555});
  auto first = EntityQuery().whereHasComponent<Mark>().gen_random();
  RandomEngine::get().set_seed(std::uint32_t{555});
  auto second = EntityQuery().whereHasComponent<Mark>().gen_random();
  CHECK(first.has_value() && second.has_value());
  CHECK(first.has_value() && second.has_value() &&
        first->id == second->id); // same seed -> same pick

  // empty query -> no value
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
  struct Absent : BaseComponent {};
  auto none = EntityQuery().whereHasComponent<Absent>().gen_random();
  CHECK(!none.has_value());
}

int main() {
  printf("=== RandomEngine tests ===\n\n");
  struct T { const char *n; void (*f)(); };
  T tests[] = {
    {"same_seed_same_sequence", test_same_seed_same_sequence},
    {"different_seed_differs", test_different_seed_differs},
    {"string_seed_stable", test_string_seed_stable},
    {"int_range_bounds", test_int_range_bounds},
    {"float_range_bounds", test_float_range_bounds},
    {"get_index", test_get_index},
    {"gen_random_reproducible", test_gen_random_reproducible},
  };
  for (auto &t : tests) { printf("  Running: %s\n", t.n); t.f(); }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) { printf("FAILURES: %d\n", tests_run - tests_passed); return 1; }
  printf("All checks passed!\n");
  return 0;
}
