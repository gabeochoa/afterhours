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

struct ToggleProbe : BaseComponent {
  int value = 7;
};

TEST(toggle_on_adds_and_returns_usable_ptr) {
  Entity &e = EntityHelper::createEntity();
  ToggleProbe *p = e.toggleComponent<ToggleProbe>(true);
  CHECK(p != nullptr);
  CHECK(e.has<ToggleProbe>());
  p->value = 42;
  CHECK(e.get<ToggleProbe>().value == 42);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(toggle_on_twice_keeps_existing_value) {
  Entity &e = EntityHelper::createEntity();
  e.toggleComponent<ToggleProbe>(true)->value = 42;
  ToggleProbe *p = e.toggleComponent<ToggleProbe>(true);
  CHECK(p != nullptr);
  CHECK(p->value == 42);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(toggle_off_removes) {
  Entity &e = EntityHelper::createEntity();
  e.toggleComponent<ToggleProbe>(true);
  CHECK(e.toggleComponent<ToggleProbe>(false) == nullptr);
  CHECK(!e.has<ToggleProbe>());
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(toggle_off_when_absent_is_noop) {
  Entity &e = EntityHelper::createEntity();
  CHECK(e.toggleComponent<ToggleProbe>(false) == nullptr);
  CHECK(!e.has<ToggleProbe>());
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(toggle_back_on_readds_default) {
  Entity &e = EntityHelper::createEntity();
  e.toggleComponent<ToggleProbe>(true)->value = 42;
  e.toggleComponent<ToggleProbe>(false);
  ToggleProbe *p = e.toggleComponent<ToggleProbe>(true);
  CHECK(p != nullptr);
  CHECK(p->value == 7);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

int main() {
  printf("=== component_toggle tests ===\n\n");
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
