// The filter behind expect_drawn / expect_not_drawn.
//
// The command handlers need a running e2e loop, so what is covered here is the
// part that decides yes or no: colour parsing and matching. An assertion that
// cannot fail is worse than no assertion, which is the bug expect_no_text had,
// so the negative cases matter more than the positive ones.

#include <cstdio>

#include "../src/plugins/e2e_testing/draw_commands.h"

using namespace afterhours;
using namespace afterhours::testing::draw_commands;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what);
  }
}

int main() {
  ColorType c{};

  check(parse_hex_color("#102030", c), "parses #rrggbb");
  check(c.r == 0x10 && c.g == 0x20 && c.b == 0x30 && c.a == 255,
        "#rrggbb defaults alpha to opaque");
  check(parse_hex_color("#04050607", c), "parses #rrggbbaa");
  check(c.a == 0x07, "#rrggbbaa keeps alpha");

  // A typo must fail the assertion, not quietly match black.
  check(!parse_hex_color("102030", c), "rejects a missing hash");
  check(!parse_hex_color("#abc", c), "rejects a short colour");
  check(!parse_hex_color("", c), "rejects empty");
  check(!parse_hex_color("#gggggg", c), "rejects non-hex");

  capture::DrawnCall call{"rectangle_outline",
                          RectangleType{1, 2, 3, 4},
                          ColorType{0x10, 0x20, 0x30, 255},
                          "",
                          7,
                          0};

  Filter by_op;
  by_op.op = "rectangle_outline";
  check(by_op.matches(call), "matches on op");

  Filter wrong_op;
  wrong_op.op = "text";
  check(!wrong_op.matches(call), "rejects a different op");

  Filter any_op;
  any_op.op = "any";
  check(any_op.matches(call), "'any' matches every op");

  Filter by_color;
  by_color.op = "rectangle_outline";
  by_color.has_color = true;
  by_color.color = ColorType{0x10, 0x20, 0x30, 255};
  check(by_color.matches(call), "matches on colour");

  // The case hanabi #61 asks for: right shape, wrong colour, must not match.
  Filter near_color = by_color;
  near_color.color = ColorType{0x10, 0x20, 0x31, 255};
  check(!near_color.matches(call), "one channel off does not match");

  Filter alpha_off = by_color;
  alpha_off.color = ColorType{0x10, 0x20, 0x30, 254};
  check(!alpha_off.matches(call), "alpha is part of the colour");

  capture::DrawnCall labelled{
      "text", RectangleType{}, ColorType{}, "Save", 9, 0};
  Filter by_text;
  by_text.op = "text";
  by_text.has_text = true;
  by_text.text = "Save";
  check(by_text.matches(labelled), "matches on text");
  by_text.text = "Sav";
  check(!by_text.matches(labelled), "text is exact, not a prefix");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  if (failures == 0) std::printf("All checks passed!\n");
  return failures == 0 ? 0 : 1;
}
