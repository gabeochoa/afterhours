#include <afterhours/src/plugins/appearance.h>

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
  printf("Running appearance tests...\n\n");

  using afterhours::os::Appearance;
  const Appearance first = afterhours::os::appearance();
  const Appearance second = afterhours::os::appearance();
  printf("  appearance: %s\n", first == Appearance::Dark    ? "Dark"
                               : first == Appearance::Light ? "Light"
                                                            : "Unknown");
  check(first == second, "the OS query is stable across calls");
  check(afterhours::os::dark_mode_enabled() == (first == Appearance::Dark),
        "dark_mode_enabled agrees with appearance");
#if defined(__APPLE__)
  check(first != Appearance::Unknown, "macOS always resolves light or dark");
#else
  check(first == Appearance::Unknown,
        "unsupported platforms report unknown appearance");
  check(!afterhours::os::dark_mode_enabled(),
        "unknown appearance is not dark mode");
#endif

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
