// window_focus_test.cpp
// is_window_focused() with no window open: false, not true. There is nothing
// that could hold focus, so the honest answer is "not focused" -- a fake true
// would let an app act as if frontmost (e.g. register a global hotkey) when
// it is not. Deliberately never calls InitWindow, mirroring
// headless_fallback_test.

#include <afterhours/src/graphics.h>

#include <cstdio>

int main() {
  bool ok = !afterhours::graphics::is_window_focused();
  printf("window focus (no window): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
