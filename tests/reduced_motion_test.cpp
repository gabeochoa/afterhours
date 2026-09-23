#include <afterhours/src/plugins/reduced_motion.h>

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
  printf("Running reduced motion tests...\n\n");

  const bool first = afterhours::os::reduced_motion_enabled();
  const bool second = afterhours::os::reduced_motion_enabled();
  check(first == second, "the OS query is stable across calls");
#if !defined(__APPLE__)
  check(!first, "unsupported platforms report no reduced motion");
#endif

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
