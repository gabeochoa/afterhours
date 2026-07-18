// entity_query_test.cpp
// Regression tests for EntityQuery take()/first() count semantics.
//
// Build (from the afterhours repo root):
//   clang++ -std=c++23 -I.. -Ivendor examples/entity_query_test.cpp -o /tmp/t && /tmp/t
//
// Covers the Limit off-by-one: the filter used `amount_taken > amount`, so
// take(n) let through n+1 entities and first() returned 2. Fixed to `>=`.

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
  if (cond)
    tests_passed++;
  else
    fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
}
#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

struct Tag : BaseComponent {};

// Build a collection with `count` entities that each have a Tag component.
static EntityCollection make_tagged(int count) {
  EntityCollection ec;
  for (int i = 0; i < count; ++i)
    ec.createEntity().addComponent<Tag>();
  ec.cleanup(); // merge temp -> main so queries see them
  return ec;
}

// take(n) yields exactly n entities (not n+1).
TEST(take_returns_exact_count) {
  EntityCollection ec = make_tagged(5);
  auto three = EntityQuery(ec).whereHasComponent<Tag>().take(3).gen();
  CHECK(three.size() == 3);

  auto one = EntityQuery(ec).whereHasComponent<Tag>().take(1).gen();
  CHECK(one.size() == 1);
}

// first() returns exactly one entity.
TEST(first_returns_one) {
  EntityCollection ec = make_tagged(4);
  auto f = EntityQuery(ec).whereHasComponent<Tag>().first().gen();
  CHECK(f.size() == 1);
}

// take(n) with more requested than available returns all available.
TEST(take_more_than_available) {
  EntityCollection ec = make_tagged(2);
  auto all = EntityQuery(ec).whereHasComponent<Tag>().take(10).gen();
  CHECK(all.size() == 2);
}

int main() {
  printf("=== entity query tests ===\n\n");
  for (auto &[name, fn] : test_registry()) {
    printf("  Running: %s\n", name);
    fn();
  }
  printf("\n%d/%d checks passed\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    printf("FAILURES: %d\n", tests_run - tests_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
