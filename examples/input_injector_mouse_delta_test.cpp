#include <afterhours/src/plugins/e2e_testing/input_injector.h>
#include <afterhours/src/plugins/e2e_testing/test_input.h>

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

TEST(scheduled_click_sets_press_and_release_edges) {
  input_injector::reset_all();
  input_injector::schedule_click_at(100.0f, 200.0f, 40.0f, 20.0f);

  input_injector::inject_scheduled_click();
  auto p = input_injector::get_mouse_position();
  CHECK(p.x == 120.0f);
  CHECK(p.y == 210.0f);
  CHECK(input_injector::is_mouse_button_pressed());
  CHECK(input_injector::is_mouse_button_down());

  input_injector::release_scheduled_click();
  CHECK(!input_injector::is_mouse_button_down());
  CHECK(input_injector::is_mouse_button_released());
}

TEST(mouse_wheel_is_one_shot) {
  input_injector::reset_all();
  CHECK(!input_injector::detail::mouse.active);
  input_injector::set_mouse_wheel(1.5f, -2.0f);
  CHECK(input_injector::detail::mouse.active);

  auto w1 = input_injector::consume_wheel();
  CHECK(w1.x == 1.5f);
  CHECK(w1.y == -2.0f);

  auto w2 = input_injector::consume_wheel();
  CHECK(w2.x == 1.5f);
  CHECK(w2.y == -2.0f);

  input_injector::reset_frame();
  auto w3 = input_injector::consume_wheel();
  CHECK(w3.x == 0.0f);
  CHECK(w3.y == 0.0f);
}

TEST(last_move_wins_within_frame) {
  input_injector::reset_all();
  input_injector::set_mouse_position(0.0f, 0.0f);
  (void)input_injector::consume_mouse_delta();

  input_injector::set_mouse_position(5.0f, 7.0f);
  input_injector::set_mouse_position(9.0f, 14.0f);
  auto d = input_injector::consume_mouse_delta();
  CHECK(d.x == 4.0f);
  CHECK(d.y == 7.0f);
}

TEST(release_clears_pressed_edge_same_frame) {
  input_injector::reset_all();
  input_injector::schedule_click_at(0.0f, 0.0f, 10.0f, 10.0f);
  input_injector::inject_scheduled_click();
  CHECK(input_injector::is_mouse_button_pressed());
  CHECK(input_injector::is_mouse_button_down());

  input_injector::release_scheduled_click();
  CHECK(!input_injector::is_mouse_button_down());
  CHECK(input_injector::is_mouse_button_released());
  // Press/release should not both be edge-active after release.
  CHECK(!input_injector::is_mouse_button_pressed());
}

TEST(simulate_click_multi_frame_lifecycle) {
  using namespace afterhours::testing::test_input;
  detail::test_mode = true;
  reset_all();

  simulate_click(50.0f, 60.0f);
  CHECK(input_injector::detail::mouse.left_down);
  CHECK(input_injector::detail::mouse.just_pressed);
  CHECK(!input_injector::detail::mouse.just_released);

  // Frame advance: click still presents as just_pressed by design.
  reset_frame();
  CHECK(input_injector::detail::mouse.left_down);
  CHECK(input_injector::detail::mouse.just_pressed);
  CHECK(!input_injector::detail::mouse.just_released);

  // Next frame: auto-release should occur.
  reset_frame();
  CHECK(!input_injector::detail::mouse.left_down);
  CHECK(!input_injector::detail::mouse.just_pressed);
  CHECK(input_injector::detail::mouse.just_released);

  detail::test_mode = false;
}

TEST(manual_release_before_reset_does_not_reassert_pressed) {
  using namespace afterhours::testing::test_input;
  detail::test_mode = true;
  reset_all();

  simulate_mouse_press();
  CHECK(input_injector::detail::mouse.left_down);
  CHECK(input_injector::detail::mouse.just_pressed);

  // Manual release in the same frame should win.
  simulate_mouse_release();
  CHECK(!input_injector::detail::mouse.left_down);
  CHECK(input_injector::detail::mouse.just_released);

  reset_frame();
  CHECK(!input_injector::detail::mouse.left_down);
  CHECK(!input_injector::detail::mouse.just_pressed);

  detail::test_mode = false;
}

TEST(key_press_is_deferred_until_next_frame) {
  input_injector::reset_all();
  constexpr int k = 65; // 'A'

  input_injector::set_key_down(k);
  CHECK(!input_injector::consume_press(k));

  input_injector::reset_frame();
  CHECK(input_injector::consume_press(k));

  input_injector::reset_frame();
  CHECK(!input_injector::consume_press(k));
}

TEST(char_poll_does_not_drop_pending_key) {
  using namespace afterhours::testing::test_input;
  detail::test_mode = true;
  reset_all();

  push_key(afterhours::keys::TAB);
  push_char('x');

  // Char polling should not destroy queued non-char events.
  int c0 = get_char_pressed([]() { return 0; });
  CHECK(c0 == 0);
  CHECK(is_key_pressed(afterhours::keys::TAB, [](int) { return false; }));
  int c1 = get_char_pressed([]() { return 0; });
  CHECK(c1 == static_cast<int>('x'));

  detail::test_mode = false;
}

TEST(wheel_visible_to_multiple_readers_same_frame) {
  input_injector::reset_all();
  input_injector::set_mouse_wheel(2.0f, -3.0f);

  auto a = input_injector::consume_wheel();
  auto b = input_injector::consume_wheel();
  CHECK(a.x == 2.0f && a.y == -3.0f);
  CHECK(b.x == 2.0f && b.y == -3.0f);
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
