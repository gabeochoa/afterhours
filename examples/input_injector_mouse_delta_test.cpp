#include <afterhours/src/plugins/e2e_testing/input_injector.h>

#include <cstdio>
#include <vector>

using namespace afterhours::testing;

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
    std::fprintf(stderr, "  FAIL: %s  (%s:%d)\n", expr, file, line);
  }
}

#define CHECK(expr) check((expr), #expr, __FILE__, __LINE__)

TEST(first_set_has_zero_delta) {
  input_injector::reset_all();
  input_injector::set_mouse_position(100.0f, 120.0f);

  auto d = input_injector::consume_mouse_delta();
  CHECK(d.x == 0.0f);
  CHECK(d.y == 0.0f);

  // Consumed deltas are one-shot.
  auto d2 = input_injector::consume_mouse_delta();
  CHECK(d2.x == 0.0f);
  CHECK(d2.y == 0.0f);
}

TEST(move_produces_delta_once) {
  input_injector::reset_all();
  input_injector::set_mouse_position(10.0f, 15.0f);
  (void)input_injector::consume_mouse_delta();

  input_injector::set_mouse_position(17.0f, 25.0f);
  auto d = input_injector::consume_mouse_delta();
  CHECK(d.x == 7.0f);
  CHECK(d.y == 10.0f);

  auto d2 = input_injector::consume_mouse_delta();
  CHECK(d2.x == 0.0f);
  CHECK(d2.y == 0.0f);
}

TEST(reset_frame_clears_unconsumed_delta) {
  input_injector::reset_all();
  input_injector::set_mouse_position(20.0f, 30.0f);
  (void)input_injector::consume_mouse_delta();
  input_injector::set_mouse_position(24.0f, 36.0f);

  input_injector::reset_frame();
  auto d = input_injector::consume_mouse_delta();
  CHECK(d.x == 0.0f);
  CHECK(d.y == 0.0f);
}

int main() {
  std::printf("Running input injector mouse delta tests...\n\n");
  for (const auto &entry : test_registry()) {
    std::printf("  %s\n", entry.name);
    entry.fn();
  }

  std::printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
  if (tests_passed != tests_run) {
    std::printf("SOME TESTS FAILED!\n");
    return 1;
  }
  std::printf("ALL TESTS PASSED.\n");
  return 0;
}
