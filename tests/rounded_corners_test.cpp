#include <afterhours/src/plugins/ui/rounded_corners.h>

#include <cstdio>
#include <string>

// #81: the corner bits were named for the opposite corner. The backends
// (raylib DrawRectangleCustom argument order, sokol fan) share one layout --
// bit 3 = top-left, 2 = top-right, 1 = bottom-left, 0 = bottom-right -- and
// every UI bitset flows to them unchanged, so the named API must agree.
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
  using namespace afterhours::ui::imm;

  {
    RoundedCorners c{std::bitset<4>{}};
    c.top_left(ROUND);
    check(c.get() == std::bitset<4>(0b1000), "top_left sets bit 3 only");
  }
  {
    RoundedCorners c{std::bitset<4>{}};
    c.top_right(ROUND);
    check(c.get() == std::bitset<4>(0b0100), "top_right sets bit 2 only");
  }
  {
    RoundedCorners c{std::bitset<4>{}};
    c.bottom_left(ROUND);
    check(c.get() == std::bitset<4>(0b0010), "bottom_left sets bit 1 only");
  }
  {
    RoundedCorners c{std::bitset<4>{}};
    c.bottom_right(ROUND);
    check(c.get() == std::bitset<4>(0b0001), "bottom_right sets bit 0 only");
  }
  {
    RoundedCorners c;
    check(c.get().all(), "default is all corners rounded");
    c.sharp(TOP_LEFT);
    check(!c.get().test(3) && c.get().count() == 3,
          "sharp(TOP_LEFT) clears the top-left bit");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
