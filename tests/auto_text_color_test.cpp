// Auto-text fallback parity across rendering backends: the monochrome and
// bi-chromatic auto_text_color resolutions must return the same RGB no
// matter which backend this TU was built with, so a screen looks identical
// on raylib and sokol. Registered in both ALL_TESTS and RAYLIB_TESTS.

#include <cmath>
#include <cstdio>

#include "../src/plugins/color.h"

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static bool eq(Color a, Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void check(Color got, Color want, const char *what) {
  checks++;
  if (!eq(got, want)) {
    failures++;
    std::printf("  FAIL: %s (got %d,%d,%d, want %d,%d,%d)\n", what,
                got.r, got.g, got.b, want.r, want.g, want.b);
  }
}

int main() {
  const Color raywhite{245, 245, 245, 255};
  const Color black{0, 0, 0, 255};

  check(colors::UI_WHITE, raywhite, "UI_WHITE is backend-independent #F5F5F5");
  check(colors::UI_BLACK, black, "UI_BLACK is black");

  // Dark backgrounds (luminance <= 0.179) resolve to the white fallback.
  for (const Color bg : {Color{0, 0, 0, 255}, Color{80, 80, 80, 255},
                         Color{115, 115, 115, 255}, Color{12, 27, 51, 255},
                         Color{110, 110, 110, 255}}) {
    check(colors::auto_text_color(bg), raywhite, "auto text on dark bg");
  }

  // Light backgrounds resolve to black.
  for (const Color bg : {Color{255, 255, 255, 255}, Color{130, 130, 130, 255},
                         Color{150, 150, 150, 255}}) {
    check(colors::auto_text_color(bg), black, "auto text on light bg");
  }

  // Near-threshold pin: #737373 lum 0.1714 picks the white fallback
  // (4.35:1). True white would read 4.74:1 — this is the parity value.
  const Color gray{115, 115, 115, 255};
  float ratio = colors::contrast_ratio(raywhite, gray);
  checks++;
  if (std::fabs(ratio - 4.35f) > 0.02f) {
    failures++;
    std::printf("  FAIL: #F5F5F5 on #737373 (got %.3f, want 4.35)\n", ratio);
  }

  // Bi-chromatic overload falls back to the same white when neither option
  // meets the minimum contrast, on a bg where the white fallback wins.
  check(colors::auto_text_color(Color{80, 80, 80, 255},
                                Color{60, 60, 60, 255}, Color{70, 70, 70, 255}),
        raywhite, "bi-chromatic fallback is the shared white");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures != 0;
}
