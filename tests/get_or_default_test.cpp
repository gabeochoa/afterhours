// get_or_default_test.cpp
//
// Entity::get_or_default(&T::member, fallback) -- read a field off a component
// that may not be attached, without a has/get ternary at every call site.
//
// The fallback is the caller's, deliberately: puzzle reads
// HasDelayConfig::frames with a fallback of 15 in one place and 0 in another,
// so no single value implied by the type could serve both.
//
// Build (from afterhours/tests/):
//   make get_or_default_test
// Run:
//   ./get_or_default_test

#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <afterhours/ah.h>

#include <cstdio>
#include <type_traits>
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

struct UIState : BaseComponent {
  float z = 0.f;
  bool selected = false;
};

struct DelayConfig : BaseComponent {
  int frames = 15;

  DelayConfig() = default;
  explicit DelayConfig(int frames_) : frames(frames_) {}
};

struct Absent : BaseComponent {
  int value = -1;
};

// ============================================================================
// Present / absent
// ============================================================================

TEST(attached_component_wins_over_the_fallback) {
  Entity &e = EntityHelper::createEntity();
  e.addComponent<DelayConfig>(3);

  // Attached wins; the fallback is not consulted.
  CHECK(e.get_or_default(&DelayConfig::frames, 0) == 3);
  CHECK(e.get_or_default(&DelayConfig::frames, 99) == 3);

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(missing_component_returns_the_fallback) {
  Entity &e = EntityHelper::createEntity();

  CHECK(e.get_or_default(&UIState::z, 4.5f) == 4.5f);
  CHECK(e.get_or_default(&Absent::value, 7) == 7);

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(fallback_is_the_callers_not_the_types_default) {
  Entity &e = EntityHelper::createEntity();

  // DelayConfig::frames default-initializes to 15, but two real callers want
  // different things for a missing component, so neither can be implied.
  CHECK(e.get_or_default(&DelayConfig::frames, 0) == 0);
  CHECK(e.get_or_default(&DelayConfig::frames, 15) == 15);

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

TEST(works_through_a_const_entity_reference) {
  Entity &e = EntityHelper::createEntity();
  const Entity &ce = e;

  CHECK(ce.get_or_default(&DelayConfig::frames, 0) == 0);
  e.addComponent<DelayConfig>(8);
  CHECK(ce.get_or_default(&DelayConfig::frames, 0) == 8);

  EntityHelper::delete_all_entities_NO_REALLY_I_MEAN_ALL();
}

// ============================================================================
// Main
// ============================================================================

int main() {
  printf("=== get_or_default tests ===\n\n");

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
