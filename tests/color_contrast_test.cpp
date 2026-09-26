// The public contrast helper a custom widget calls: colors::contrast_ratio
// from color.h alone, no UI headers. Pins the WCAG values so the function
// cannot quietly change formula or sink back behind an internal header.

#include <cmath>
#include <cstdio>

#include "../src/plugins/color.h"

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const char *what) {
  checks++;
  if (!cond) {
    failures++;
    std::printf("  FAIL: %s\n", what);
  }
}

static void check_approx(float got, float want, const char *what) {
  checks++;
  if (std::fabs(got - want) > 0.01f) {
    failures++;
    std::printf("  FAIL: %s (got %.3f, want %.3f)\n", what, got, want);
  }
}

int main() {
  const Color black{0, 0, 0, 255};
  const Color white{255, 255, 255, 255};
  const Color aa_gray{0x76, 0x76, 0x76, 255};

  check_approx(colors::contrast_ratio(black, white), 21.f,
               "black on white is 21:1");
  check_approx(colors::contrast_ratio(white, black), 21.f,
               "ratio is symmetric");
  check_approx(colors::contrast_ratio(aa_gray, aa_gray), 1.f,
               "a colour against itself is 1:1");
  check_approx(colors::contrast_ratio(aa_gray, white), 4.54f,
               "#767676 on white is 4.54:1 (the AA boundary grey)");
  check(colors::wcag_compliance(black, white) == colors::WCAGLevel::AAA,
        "black on white reaches AAA");
  check_approx(colors::luminance(white), 1.f, "white luminance is 1");
  check_approx(colors::luminance(black), 0.f, "black luminance is 0");

  std::printf("%d/%d checks passed\n", checks - failures, checks);
  return failures != 0;
}
