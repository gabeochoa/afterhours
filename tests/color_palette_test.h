#include <afterhours/src/plugins/color.h>

#include <cstdio>

static int checks_run = 0;
static int checks_passed = 0;

static void check(const afterhours::Color &c, int r, int g, int b, int a,
                  const char *what) {
  checks_run++;
  if (c.r == r && c.g == g && c.b == b && c.a == a) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s is %d,%d,%d,%d, expected %d,%d,%d,%d\n", what,
            c.r, c.g, c.b, c.a, r, g, b, a);
  }
}

int main() {
  using namespace afterhours;

  check(colors::LIGHTGRAY, 200, 200, 200, 255, "LIGHTGRAY");
  check(colors::GRAY, 130, 130, 130, 255, "GRAY");
  check(colors::DARKGRAY, 80, 80, 80, 255, "DARKGRAY");
  check(colors::YELLOW, 253, 249, 0, 255, "YELLOW");
  check(colors::GOLD, 255, 203, 0, 255, "GOLD");
  check(colors::ORANGE, 255, 161, 0, 255, "ORANGE");
  check(colors::PINK, 255, 109, 194, 255, "PINK");
  check(colors::RED, 230, 41, 55, 255, "RED");
  check(colors::MAROON, 190, 33, 55, 255, "MAROON");
  check(colors::GREEN, 0, 228, 48, 255, "GREEN");
  check(colors::LIME, 0, 158, 47, 255, "LIME");
  check(colors::DARKGREEN, 0, 117, 44, 255, "DARKGREEN");
  check(colors::SKYBLUE, 102, 191, 255, 255, "SKYBLUE");
  check(colors::BLUE, 0, 121, 241, 255, "BLUE");
  check(colors::DARKBLUE, 0, 82, 172, 255, "DARKBLUE");
  check(colors::PURPLE, 200, 122, 255, 255, "PURPLE");
  check(colors::VIOLET, 135, 60, 190, 255, "VIOLET");
  check(colors::DARKPURPLE, 112, 31, 126, 255, "DARKPURPLE");
  check(colors::BEIGE, 211, 176, 131, 255, "BEIGE");
  check(colors::BROWN, 127, 106, 79, 255, "BROWN");
  check(colors::DARKBROWN, 76, 63, 47, 255, "DARKBROWN");
  check(colors::WHITE, 255, 255, 255, 255, "WHITE");
  check(colors::BLACK, 0, 0, 0, 255, "BLACK");
  check(colors::BLANK, 0, 0, 0, 0, "BLANK");
  check(colors::MAGENTA, 255, 0, 255, 255, "MAGENTA");
  check(colors::RAYWHITE, 245, 245, 245, 255, "RAYWHITE");

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
