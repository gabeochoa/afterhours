#include <afterhours/src/graphics.h>

#include <cstdio>

int main() {
  bool ok = !afterhours::graphics::is_window_focused();
  printf("window focus (no window): %s\n", ok ? "ok" : "FAIL");
  return ok ? 0 : 1;
}
