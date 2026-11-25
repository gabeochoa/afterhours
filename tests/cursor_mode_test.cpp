#if defined(AFTER_HOURS_USE_NONE) || defined(AFTER_HOURS_USE_PSX)
#include <afterhours/src/graphics_common.h>
#else
#include <afterhours/src/graphics.h>
#endif

#if defined(AFTER_HOURS_SOKOL)
#include <afterhours/src/backends/sokol/drawing_helpers.h>
#endif

#include <cstdio>

namespace g = afterhours::graphics;

int main() {
  int failures = 0;
  auto check = [&](bool cond, const char *what) {
    if (!cond) {
      failures++;
      std::printf("  FAIL: %s\n", what);
    }
  };

  check(g::get_cursor_mode() == g::CursorMode::Free, "default is Free");
  g::set_cursor_mode(g::CursorMode::Free);
  check(g::get_cursor_mode() == g::CursorMode::Free, "Free repeat is a no-op");
  g::set_cursor_mode(g::CursorMode::Hidden);
  check(g::get_cursor_mode() == g::CursorMode::Hidden, "Free -> Hidden");
  g::set_cursor_mode(g::CursorMode::Hidden);
  check(g::get_cursor_mode() == g::CursorMode::Hidden,
        "Hidden repeat is a no-op");
  g::set_cursor_mode(g::CursorMode::Captured);
  check(g::get_cursor_mode() == g::CursorMode::Captured, "Hidden -> Captured");
  g::set_cursor_mode(g::CursorMode::Captured);
  check(g::get_cursor_mode() == g::CursorMode::Captured,
        "Captured repeat is a no-op");
  g::set_cursor_mode(g::CursorMode::Free);
  check(g::get_cursor_mode() == g::CursorMode::Free, "Captured -> Free");

  std::printf("cursor_mode_test: %s\n", failures ? "FAIL" : "ok");
  return failures ? 1 : 0;
}
