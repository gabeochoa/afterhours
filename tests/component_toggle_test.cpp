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

struct ToggleArgProbe : BaseComponent {
  explicit ToggleArgProbe(int v) : value(v) {}
  int value;
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

TEST(toggle_on_forwards_ctor_args_and_preserves_existing) {
  Entity &e = EntityHelper::createEntity();
  ToggleArgProbe *p = e.toggleComponent<ToggleArgProbe>(true, 11);
  CHECK(p != nullptr);
  CHECK(p->value == 11);
  ToggleArgProbe *again = e.toggleComponent<ToggleArgProbe>(true, 99);
  CHECK(again == p);
  CHECK(again->value == 11);
  CHECK(e.toggleComponent<ToggleArgProbe>(false) == nullptr);
  CHECK(!e.has<ToggleArgProbe>());
  ToggleArgProbe *fresh = e.toggleComponent<ToggleArgProbe>(true, 33);
  CHECK(fresh != nullptr);
  CHECK(fresh->value == 33);
  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

enum class ToggleTag : TagId { Alpha = 4, Beta = 5 };

TEST(toggle_tag_sets_and_clears) {
  Entity &e = EntityHelper::createEntity();
  e.toggleTag(ToggleTag::Alpha, true);
  CHECK(e.hasTag(ToggleTag::Alpha));
  e.toggleTag(ToggleTag::Alpha, true);
  CHECK(e.hasTag(ToggleTag::Alpha));
  e.toggleTag(ToggleTag::Alpha, false);
  CHECK(!e.hasTag(ToggleTag::Alpha));
  e.toggleTag(TagId{5}, true);
  CHECK(e.hasTag(ToggleTag::Beta));
  e.toggleTag(TagId{5}, false);
  CHECK(!e.hasTag(ToggleTag::Beta));
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
