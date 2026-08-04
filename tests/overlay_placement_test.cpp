// overlay_placement_test.cpp
// D13: dropdown_menu / context_menu / popover are one engine. This is the part
// all three need and all three hand-rolled downstream -- anchored placement
// with edge flipping.

#include <afterhours/src/plugins/ui/overlay.h>

#include <cstdio>
#include <string>

namespace ov = afterhours::ui::overlay;
using ov::Placement;

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

static constexpr float SW = 800.f;
static constexpr float SH = 600.f;

int main() {
  printf("=== overlay placement tests ===\n\n");

  // Room below: sits directly under the trigger, left edges aligned.
  {
    const RectangleType anchor{100, 100, 120, 30};
    const auto p = ov::place(anchor, 150, 200, SW, SH, Placement::Below);
    check(p.used == Placement::Below && !p.flipped, "stays below when it fits");
    check(p.x == 100.f && p.y == 130.f, "sits directly under the trigger");
  }

  // Near the bottom: flips above rather than running off screen.
  {
    const RectangleType anchor{100, 550, 120, 30};
    const auto p = ov::place(anchor, 150, 200, SW, SH, Placement::Below);
    check(p.used == Placement::Above && p.flipped, "flips above near the bottom");
    check(p.y == 350.f, "sits with its bottom edge on the trigger");
    check(p.y + 200.f <= SH, "stays on screen");
  }

  // Mirror case: prefer Right, flip to Left near the right edge.
  {
    const RectangleType anchor{700, 100, 60, 30};
    const auto p = ov::place(anchor, 200, 100, SW, SH, Placement::Right);
    check(p.used == Placement::Left && p.flipped, "flips left near the right edge");
    check(p.x == 500.f, "sits with its right edge on the trigger");
  }

  // Cross axis is clamped: a wide menu under a trigger near the right edge
  // still has to be fully visible.
  {
    const RectangleType anchor{760, 100, 30, 30};
    const auto p = ov::place(anchor, 200, 100, SW, SH, Placement::Below);
    check(p.used == Placement::Below, "no flip needed on the main axis");
    check(p.x == 600.f, "clamped so the right edge lands on the screen edge");
    check(p.x + 200.f <= SW, "stays on screen horizontally");
  }

  // Neither side fits and the preferred one is still roomier: stay put and
  // clamp. Flipping here would only make things worse.
  {
    const RectangleType anchor{100, 250, 120, 30}; // 320 below, 250 above
    const auto p = ov::place(anchor, 100, 500, SW, SH, Placement::Below);
    check(p.used == Placement::Below && !p.flipped,
          "keeps the preferred side when it has more room");
    check(p.y >= 0.f && p.y + 500.f <= SH, "clamped fully on screen");
  }

  // Neither side fits and the opposite is roomier: flip, then clamp.
  {
    const RectangleType anchor{100, 500, 120, 30}; // 70 below, 500 above
    const auto p = ov::place(anchor, 100, 550, SW, SH, Placement::Below);
    check(p.used == Placement::Above && p.flipped,
          "takes the roomier side when neither fits");
    check(p.y >= 0.f && p.y + 550.f <= SH, "clamped fully on screen");
  }

  // A gap is honoured, and does not break the flip decision.
  {
    const RectangleType anchor{100, 100, 120, 30};
    const auto p = ov::place(anchor, 150, 200, SW, SH, Placement::Below, 8.f);
    check(p.y == 138.f, "gap offsets the overlay from the trigger");
  }

  // Degenerate: an overlay larger than the screen clamps to the origin instead
  // of producing a negative position.
  {
    const RectangleType anchor{400, 300, 10, 10};
    const auto p = ov::place(anchor, 1000, 900, SW, SH, Placement::Below);
    check(p.x == 0.f && p.y == 0.f, "an oversized overlay clamps to the origin");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
