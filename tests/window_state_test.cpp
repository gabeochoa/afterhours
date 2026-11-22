#include <afterhours/src/graphics.h>

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

  g::clear_window_state(g::FLAG_WINDOW_RESIZABLE | g::FLAG_VSYNC_HINT);
  check(g::window_state() == 0, "clear leaves no tracked bits");
  g::set_window_state(g::FLAG_WINDOW_RESIZABLE);
  check((g::window_state() & g::FLAG_WINDOW_RESIZABLE) != 0,
        "set records RESIZABLE");
  check((g::window_state() & g::FLAG_VSYNC_HINT) == 0,
        "VSYNC stays clear");
  g::set_window_state(g::FLAG_VSYNC_HINT);
  g::clear_window_state(g::FLAG_WINDOW_RESIZABLE);
  check((g::window_state() & g::FLAG_VSYNC_HINT) != 0,
        "VSYNC survives clearing RESIZABLE");
  check((g::window_state() & g::FLAG_WINDOW_RESIZABLE) == 0,
        "RESIZABLE stays clear");

  std::printf("window_state_test: %s\n", failures ? "FAIL" : "ok");
  return failures ? 1 : 0;
}
