// sokol_include_order_test.cpp
// Includes window_manager.h under SOKOL_METAL, which nothing else here did.
//
// That gap let a real one through: backend.h's get_mouse_position called
// window_manager::window_to_content, but window_manager.h includes graphics.h
// (and so backend.h) before it declares that struct. Every sokol/Metal build
// failed with "use of undeclared identifier 'window_manager'" for 28 commits,
// because wm is raylib and the two sokol tests here only reach for graphics.h.
//
// So this test is mostly its own include list. The checks below are there to
// give it something to assert; the compile is the point.

#define AFTER_HOURS_USE_METAL

// window_manager.h first and on its own: that ordering is what broke.
#include <afterhours/src/plugins/window_manager.h>

#include <afterhours/src/graphics.h>
#include <afterhours/src/backends/sokol/drawing_helpers.h>
#include <afterhours/src/plugins/input_system.h>

#include <cmath>
#include <cstdio>
#include <string>

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

int main() {
  printf("=== sokol include order ===\n\n");

  using WM = afterhours::window_manager;

  // The letterbox itself is viewport_test's job. All this needs is to call it
  // through the include path that used to fail, so a round trip will do.
  {
    const Vector2Type p{640.f, 360.f};
    const Vector2Type there = WM::window_to_content(p, 1280, 720);
    const Vector2Type back = WM::content_to_window(there, 1280, 720);
    printf("  %.0f,%.0f -> %.0f,%.0f -> %.0f,%.0f\n", p.x, p.y, there.x,
           there.y, back.x, back.y);
    check(std::abs(back.x - p.x) < 0.5f && std::abs(back.y - p.y) < 0.5f,
          "window_to_content round trips through content_to_window");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
