#define AFTER_HOURS_USE_NONE 1
#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>

#include <cstdio>

using afterhours::input;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const char *expr, int line) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    std::fprintf(stderr, "  FAIL: %s (line %d)\n", expr, line);
  }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

int main() {
  std::printf("Running multi-click tests...\n\n");

  input::MultiClick clicks;
  CHECK(clicks.update(true, Vector2Type{100.f, 100.f}, 1.0f) == 1);
  CHECK(clicks.count() == 1);
  CHECK(clicks.update(true, Vector2Type{102.f, 101.f}, 1.2f) == 2);
  CHECK(clicks.update(true, Vector2Type{101.f, 100.f}, 1.3f) == 3);
  CHECK(clicks.count() == 3);
  CHECK(clicks.update(true, Vector2Type{100.f, 100.f}, 1.4f) == 1);
  CHECK(clicks.update(true, Vector2Type{100.f, 100.f}, 1.5f) == 2);

  input::MultiClick idle;
  CHECK(idle.update(true, Vector2Type{100.f, 100.f}, 1.0f) == 1);
  CHECK(idle.update(false, Vector2Type{500.f, 500.f}, 1.1f) == 0);
  CHECK(idle.count() == 1);
  CHECK(idle.update(true, Vector2Type{101.f, 100.f}, 1.2f) == 2);

  input::MultiClick timed_out;
  CHECK(timed_out.update(true, Vector2Type{10.f, 10.f}, 1.0f) == 1);
  CHECK(timed_out.update(true, Vector2Type{10.f, 10.f}, 1.41f) == 1);
  CHECK(timed_out.update(true, Vector2Type{10.f, 10.f}, 1.5f) == 2);

  input::MultiClick moved;
  CHECK(moved.update(true, Vector2Type{10.f, 10.f}, 1.0f) == 1);
  CHECK(moved.update(true, Vector2Type{17.f, 10.f}, 1.1f) == 1);
  CHECK(moved.update(true, Vector2Type{18.f, 10.f}, 1.2f) == 2);

  input::MultiClick at_slop;
  CHECK(at_slop.update(true, Vector2Type{10.f, 10.f}, 1.0f) == 1);
  CHECK(at_slop.update(true, Vector2Type{16.f, 10.f}, 1.1f) == 2);

  input::MultiClick capped{.max_clicks = 2};
  CHECK(capped.update(true, Vector2Type{0.f, 0.f}, 1.0f) == 1);
  CHECK(capped.update(true, Vector2Type{0.f, 0.f}, 1.1f) == 2);
  CHECK(capped.update(true, Vector2Type{0.f, 0.f}, 1.2f) == 1);

  input::MultiClick resettable;
  CHECK(resettable.update(true, Vector2Type{10.f, 10.f}, 1.0f) == 1);
  CHECK(resettable.update(true, Vector2Type{10.f, 10.f}, 1.1f) == 2);
  resettable.reset();
  CHECK(resettable.count() == 0);
  CHECK(resettable.update(true, Vector2Type{10.f, 10.f}, 1.2f) == 1);

  std::printf("\n%d/%d checks passed.\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    std::printf("SOME TESTS FAILED!\n");
    return 1;
  }
  std::printf("ALL TESTS PASSED.\n");
  return 0;
}
