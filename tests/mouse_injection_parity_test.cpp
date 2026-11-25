#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/input_system.h>

#include <cstdio>

namespace gfx = afterhours::graphics;
namespace inj = afterhours::testing::input_injector;
namespace test_input = afterhours::testing::test_input;
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

static void reset_test_mode() {
  test_input::detail::test_mode = true;
  test_input::reset_all();
  inj::reset_all();
}

int main() {
  std::printf("Running mouse injection parity tests...\n\n");

  reset_test_mode();
  CHECK(!inj::detail::mouse.active);
  auto backend_true = [](int) { return true; };
  CHECK(!test_input::is_mouse_button_down(0, backend_true));
  CHECK(!test_input::is_mouse_button_pressed(0, backend_true));
  CHECK(!test_input::is_mouse_button_released(0, backend_true));
  CHECK(test_input::is_mouse_button_up(0, backend_true));
  CHECK(test_input::get_mouse_wheel_move([] { return 99.f; }) == 0.f);
  auto sentinel_pos = test_input::get_mouse_position<raylib::Vector2>(
      [] { return raylib::Vector2{999.f, 999.f}; });
  CHECK(sentinel_pos.x == 0.f && sentinel_pos.y == 0.f);

  CHECK(!input::is_mouse_button_down(0));
  CHECK(!input::is_mouse_button_down(1));
  CHECK(!input::is_mouse_button_down(2));
  CHECK(input::is_mouse_button_up(0));
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_released(0));
  auto parked = input::get_mouse_position();
  CHECK(parked.x == 0.f && parked.y == 0.f);
  auto zero_delta = input::get_mouse_delta();
  CHECK(zero_delta.x == 0.f && zero_delta.y == 0.f);
  auto zero_delta_again = input::get_mouse_delta();
  CHECK(zero_delta_again.x == 0.f && zero_delta_again.y == 0.f);
  CHECK(!gfx::is_mouse_button_down(0));
  CHECK(gfx::is_mouse_button_up(0));
  CHECK(gfx::get_mouse_wheel_move() == 0.f);
  auto gfx_parked = gfx::get_mouse_position();
  CHECK(gfx_parked.x == 0.f && gfx_parked.y == 0.f);

  inj::set_mouse_position(10.f, 10.f);
  inj::set_mouse_position(17.f, 25.f);
  inj::set_mouse_position(20.f, 30.f);
  auto delta = input::get_mouse_delta();
  CHECK(delta.x == 10.f && delta.y == 20.f);
  auto delta_again = input::get_mouse_delta();
  CHECK(delta_again.x == 10.f && delta_again.y == 20.f);
  auto injector_delta = inj::consume_mouse_delta();
  CHECK(injector_delta.x == 10.f && injector_delta.y == 20.f);
  test_input::reset_frame();
  auto cleared_delta = input::get_mouse_delta();
  CHECK(cleared_delta.x == 0.f && cleared_delta.y == 0.f);
  auto kept_pos = input::get_mouse_position();
  CHECK(kept_pos.x == 20.f && kept_pos.y == 30.f);

  reset_test_mode();
  test_input::set_mouse_position(123.f, 45.f);
  auto input_pos = input::get_mouse_position();
  CHECK(input_pos.x == 123.f && input_pos.y == 45.f);
  auto gfx_pos = gfx::get_mouse_position();
  CHECK(gfx_pos.x == 123.f && gfx_pos.y == 45.f);

  inj::set_mouse_wheel(0.f, 2.5f);
  CHECK(gfx::get_mouse_wheel_move() == 2.5f);
  CHECK(gfx::get_mouse_wheel_move() == 2.5f);
  test_input::reset_frame();

  test_input::simulate_click(123.f, 45.f);
  CHECK(gfx::is_mouse_button_down(0));
  CHECK(gfx::is_mouse_button_pressed(0));
  CHECK(!gfx::is_mouse_button_up(0));
  CHECK(input::is_mouse_button_down(0));
  CHECK(input::is_mouse_button_pressed(0));

  test_input::reset_frame();
  CHECK(gfx::is_mouse_button_down(0));

  test_input::reset_frame();
  CHECK(!gfx::is_mouse_button_down(0));
  CHECK(gfx::is_mouse_button_up(0));
  CHECK(gfx::is_mouse_button_released(0));
  CHECK(input::is_mouse_button_released(0));

  test_input::detail::test_mode = false;
  test_input::reset_all();

  std::printf("\n%d/%d checks passed.\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    std::printf("SOME TESTS FAILED!\n");
    return 1;
  }
  std::printf("ALL TESTS PASSED.\n");
  return 0;
}
