#define FMT_HEADER_ONLY
#include <afterhours/src/plugins/input_system.h>
#ifdef AFTER_HOURS_USE_RAYLIB
#include "input_raylib_unused_stubs.h"
#endif

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

#ifdef AFTER_HOURS_USE_RAYLIB
namespace {
bool mouse_down[8] = {};
} // namespace

namespace raylib {
Vector2 GetMouseDelta() { return {}; }

Vector2 GetMousePosition() { return {}; }

int GetRenderWidth() { return 800; }

int GetRenderHeight() { return 600; }

bool IsGamepadAvailable(int) { return false; }

bool IsKeyPressed(int) { return false; }

bool IsKeyPressedRepeat(int) { return false; }

bool IsKeyDown(int) { return false; }

int GetCharPressed() { return 0; }

bool IsMouseButtonDown(int button) {
  return button >= 0 && button < 8 && mouse_down[button];
}

bool IsMouseButtonUp(int button) { return !IsMouseButtonDown(button); }

bool IsMouseButtonReleased(int) { return false; }

float GetMouseWheelMove() { return 0.f; }

Vector2 GetMouseWheelMoveV() { return {}; }

bool IsGamepadButtonPressed(int, int) { return false; }

bool IsGamepadButtonDown(int, int) { return false; }

float GetGamepadAxisMovement(int, int) { return 0.f; }
} // namespace raylib

static void reset_edges() {
  for (bool &down : mouse_down) down = false;
  input::mouse_button_edge_state() = {};
  input::update_mouse_button_edges();
}
#else
static void reset_edges() { input::mouse_button_edge_state() = {}; }
#endif

int main() {
  std::printf("Running mouse press edge tests...\n\n");

  input::MouseButtonEdgeState state;
  state.current[0] = true;
  CHECK(state.pressed(0));
  CHECK(state.pressed(0));
  CHECK(state.current[0] && !state.previous[0]);
  CHECK(!state.pressed(1));
  CHECK(!state.pressed(-1));
  CHECK(!state.pressed(8));

  reset_edges();
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(0));

#ifdef AFTER_HOURS_USE_RAYLIB
  mouse_down[0] = true;
  input::update_mouse_button_edges();
  CHECK(input::is_mouse_button_pressed(0));
  CHECK(input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(1));

  input::update_mouse_button_edges();
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(input::is_mouse_button_down(0));

  reset_edges();
  mouse_down[0] = true;
  input::update_mouse_button_edges();
  mouse_down[0] = false;
  input::update_mouse_button_edges();
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(0));

  mouse_down[0] = true;
  input::update_mouse_button_edges();
  CHECK(input::is_mouse_button_pressed(0));
  mouse_down[0] = false;
  input::update_mouse_button_edges();
  CHECK(!input::is_mouse_button_pressed(0));
  mouse_down[0] = true;
  input::update_mouse_button_edges();
  CHECK(input::is_mouse_button_pressed(0));
  CHECK(input::is_mouse_button_pressed(0));

  reset_edges();
  afterhours::Entity &entity = afterhours::EntityHelper::createPermanentEntity();
  input::add_singleton_components(entity,
                                  input::ProvidesInputMapping::GameMapping{});
  input::InputSystem system;
  auto frame = [&] {
    system.for_each_with(entity, entity.get<input::InputCollector>(),
                         entity.get<input::ProvidesMaxGamepadID>(),
                         entity.get<input::ProvidesInputMapping>(),
                         1.f / 60.f);
  };

  mouse_down[0] = true;
  frame();
  CHECK(input::is_mouse_button_pressed(0));
  CHECK(input::is_mouse_button_pressed(0));

  frame();
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(0));
#else
  input::update_mouse_button_edges();
  CHECK(!input::is_mouse_button_pressed(0));
  CHECK(!input::is_mouse_button_pressed(0));
#endif

  std::printf("\n%d/%d checks passed.\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    std::printf("SOME TESTS FAILED!\n");
    return 1;
  }
  std::printf("ALL TESTS PASSED.\n");
  return 0;
}
