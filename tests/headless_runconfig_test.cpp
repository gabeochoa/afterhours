// headless_runconfig_test.cpp
// --headless was parsed and then dropped: run() called init_window
// regardless. The wiring is what broke, so these check the wiring.

#define AFTER_HOURS_USE_RAYLIB

#include <afterhours/src/graphics_common.h>
#include <afterhours/src/plugins/e2e_testing/harness.h>

#include <cstdio>
#include <string>
#include <vector>

using afterhours::graphics::DisplayMode;
using afterhours::graphics::RunConfig;
using afterhours::testing::E2EArgs;

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
  printf("=== headless RunConfig wiring ===\n\n");

  {
    const RunConfig cfg;
    check(cfg.display == DisplayMode::Windowed,
          "the default is still a window, so this is additive");
  }

  {
    RunConfig cfg;
    cfg.display = DisplayMode::Headless;
    check(cfg.display == DisplayMode::Headless,
          "RunConfig carries a display mode at all");
  }

  {
    E2EArgs args;
    check(args.display_mode() == DisplayMode::Windowed,
          "unset --headless asks for a window");

    args.headless = true;
    check(args.display_mode() == DisplayMode::Headless,
          "--headless asks for no window");
  }

  // Through the real parser: parsing and using were never connected.
  {
    const char *argv[] = {"app", "--test-script-dir", "scripts", "--headless"};
    const E2EArgs parsed =
        afterhours::testing::parse_e2e_args(4, const_cast<char **>(argv));
    printf("  parsed --headless -> %s\n",
           parsed.display_mode() == DisplayMode::Headless ? "Headless"
                                                          : "Windowed");
    check(parsed.headless, "the parser still sets the flag");
    check(parsed.display_mode() == DisplayMode::Headless,
          "and it now reaches a display mode");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
