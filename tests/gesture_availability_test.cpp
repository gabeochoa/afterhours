#include <afterhours/src/gestures_macos.h>

#include <cstdio>
#include <string>

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
  printf("Running gesture availability tests...\n\n");

  using afterhours::gestures::Availability;
  const auto first = afterhours::gestures::gesture_availability();
  const auto second = afterhours::gestures::gesture_availability();
  check(first == second, "the availability query is stable across calls");

#if defined(__APPLE__) && defined(AFTER_HOURS_ENABLE_MACOS_GESTURES)
  check(first == Availability::Unknown,
        "before install the answer is unknown, not a claim");
  afterhours::gestures::install_pinch_monitor();
  check(afterhours::gestures::gesture_availability() ==
            Availability::Available,
        "a live monitor means pinch can be delivered");
#else
  check(first == Availability::Unavailable,
        "a build with no gesture path reports unavailable");
  afterhours::gestures::install_pinch_monitor();
  check(afterhours::gestures::gesture_availability() ==
            Availability::Unavailable,
        "installing nothing stays unavailable");
#endif

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
