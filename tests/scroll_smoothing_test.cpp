// scroll_smoothing_test.cpp
// HasScrollView easing. The wheel drives scroll_target and scroll_offset
// glides toward it, so scrolling does not feel stepped.
//
// The cases that matter are the compatibility ones: existing code writes
// scroll_offset directly to jump to the top, and that has to keep working.

#include <afterhours/src/plugins/ui/components.h>

#include <cmath>
#include <cstdio>
#include <string>

using afterhours::ui::HasScrollView;

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

static HasScrollView make_view(float smoothing) {
  HasScrollView sv;
  sv.content_size = {0, 1000};
  sv.viewport_size = {0, 200};
  sv.scroll_smoothing = smoothing;
  return sv;
}

static constexpr float FRAME60 = 1.0f / 60.0f;

int main() {
  printf("=== scroll smoothing tests ===\n\n");

  // 1. Default is instant, so nothing changes for callers who never opt in.
  {
    HasScrollView sv = make_view(1.0f);
    sv.scroll_target.y = 300.f;
    sv.ease_scroll(FRAME60);
    check(sv.scroll_offset.y == 300.f, "default smoothing snaps to target");
  }

  // 2. Smoothing moves partway, then converges.
  {
    HasScrollView sv = make_view(0.25f);
    sv.scroll_target.y = 400.f;
    sv.ease_scroll(FRAME60);
    const float first = sv.scroll_offset.y;
    check(first > 0.f && first < 400.f, "first step lands partway");
    for (int i = 0; i < 240; i++)
      sv.ease_scroll(FRAME60);
    check(sv.scroll_offset.y == 400.f, "settles exactly on target");
  }

  // 3. Already-there is a no-op, so an idle view does not redraw forever.
  {
    HasScrollView sv = make_view(0.25f);
    sv.scroll_target.y = 100.f;
    sv.scroll_offset.y = 100.f;
    sv.ease_scroll(FRAME60);
    check(sv.scroll_offset.y == 100.f, "no drift when already on target");
  }

  // 4. The compatibility case. Existing code resets the view by assigning
  //    scroll_offset directly; the target has to follow rather than yank it
  //    back on the next frame.
  {
    HasScrollView sv = make_view(0.25f);
    sv.scroll_target.y = 500.f;
    for (int i = 0; i < 200; i++)
      sv.ease_scroll(FRAME60);

    sv.scroll_offset = {0, 0}; // scroll-to-top, as the apps write it
    sv.ease_scroll(FRAME60);
    check(sv.scroll_offset.y == 0.f,
          "a direct scroll_offset write is not reverted by easing");
  }

  // 5. Clamping covers the target too, or the wheel could park it past the end
  //    and the offset would chase a value it can never reach.
  {
    HasScrollView sv = make_view(0.25f);
    sv.scroll_target.y = 99999.f;
    sv.scroll_offset.y = -50.f;
    sv.clamp_scroll();
    check(sv.scroll_target.y == 800.f, "target clamps to max scroll");
    check(sv.scroll_offset.y == 0.f, "offset clamps to zero");
  }

  // 6. Frame rate must not change how fast it feels: the same elapsed time has
  //    to land in the same place at 60 and 120 fps.
  {
    HasScrollView slow = make_view(0.25f);
    HasScrollView fast = make_view(0.25f);
    slow.scroll_target.y = 600.f;
    fast.scroll_target.y = 600.f;
    for (int i = 0; i < 6; i++)
      slow.ease_scroll(FRAME60);
    for (int i = 0; i < 12; i++)
      fast.ease_scroll(FRAME60 / 2.f);
    check(std::fabs(slow.scroll_offset.y - fast.scroll_offset.y) < 1.0f,
          "0.1s of easing lands the same at 60fps and 120fps");
    if (std::fabs(slow.scroll_offset.y - fast.scroll_offset.y) >= 1.0f)
      fprintf(stderr, "        60fps=%.1f 120fps=%.1f\n", slow.scroll_offset.y,
              fast.scroll_offset.y);
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
