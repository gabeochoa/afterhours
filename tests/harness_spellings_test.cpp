#include <afterhours/src/plugins/e2e_testing/harness.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using afterhours::testing::E2EArgs;
using afterhours::testing::parse_e2e_args;
using afterhours::testing::should_run_e2e;

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

static bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

static E2EArgs parse(std::vector<std::string> words) {
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>("app"));
  for (auto &w : words) argv.push_back(w.data());
  return parse_e2e_args(static_cast<int>(argv.size()), argv.data());
}

static bool should_run(std::vector<std::string> words) {
  std::vector<char *> argv;
  argv.push_back(const_cast<char *>("app"));
  for (auto &w : words) argv.push_back(w.data());
  return should_run_e2e(static_cast<int>(argv.size()), argv.data());
}

int main() {
  printf("=== harness argument spellings ===\n\n");

  {
    const E2EArgs a = parse({});
    check(!a.enabled && a.script_path.empty() && a.script_dir.empty() &&
              a.screenshot_dir.empty() && near(a.timeout_seconds, 30.0f) &&
              !a.slow_mode && near(a.slow_delay, 0.5f) &&
              !a.update_baselines && !a.headless && !a.quiet &&
              near(a.time_scale, 1.0f) && a.capture_interval == 0,
          "no arguments keeps every default");
  }

  {
    const E2EArgs a = parse({"--test-script", "a.e2e", "--test-script-dir",
                             "scripts", "--screenshot-dir", "shots",
                             "--timeout", "12.5", "--slow-delay", "0.25",
                             "--time-scale", "2", "--capture-interval", "3",
                             "--slow", "--update-baselines", "--headless",
                             "--quiet", "--e2e"});
    check(a.enabled && a.script_path == "a.e2e" && a.script_dir == "scripts" &&
              a.screenshot_dir == "shots" && near(a.timeout_seconds, 12.5f) &&
              a.slow_mode && near(a.slow_delay, 0.25f) &&
              near(a.time_scale, 2.0f) && a.capture_interval == 3 &&
              a.update_baselines && a.headless && a.quiet,
          "separated spellings still parse as before");
  }

  {
    const E2EArgs a = parse({"--e2e-speed", "4"});
    check(near(a.time_scale, 4.0f), "--e2e-speed separated still parses");
  }

  {
    const E2EArgs a = parse({"--test-script=a.e2e"});
    check(a.enabled && a.script_path == "a.e2e",
          "--test-script=<path> parses and enables");
  }
  {
    const E2EArgs a = parse({"--test-script-dir=scripts"});
    check(a.enabled && a.script_dir == "scripts",
          "--test-script-dir=<dir> parses and enables");
  }
  {
    const E2EArgs a = parse({"--screenshot-dir=shots"});
    check(a.screenshot_dir == "shots", "--screenshot-dir=<dir> parses");
  }
  {
    const E2EArgs a = parse({"--timeout=12.5"});
    check(near(a.timeout_seconds, 12.5f), "--timeout=<seconds> parses");
  }
  {
    const E2EArgs a = parse({"--e2e-timeout=7.5"});
    check(near(a.timeout_seconds, 7.5f), "--e2e-timeout=<seconds> parses");
  }
  {
    const E2EArgs a = parse({"--e2e-timeout", "7.5"});
    check(near(a.timeout_seconds, 7.5f),
          "--e2e-timeout <seconds> parses as the --timeout alias");
  }
  {
    const E2EArgs a = parse({"--slow-delay=0.25"});
    check(a.slow_mode && near(a.slow_delay, 0.25f),
          "--slow-delay=<seconds> parses and enables slow mode");
  }
  {
    const E2EArgs a = parse({"--time-scale=2"});
    check(near(a.time_scale, 2.0f), "--time-scale=<factor> parses");
  }
  {
    const E2EArgs a = parse({"--e2e-speed=4"});
    check(near(a.time_scale, 4.0f), "--e2e-speed=<factor> parses");
  }
  {
    const E2EArgs a = parse({"--capture-interval=3"});
    check(a.capture_interval == 3, "--capture-interval=<frames> parses");
  }

  {
    const E2EArgs a = parse({"--capture-scale", "2"});
    check(a.capture_scale == 2, "--capture-scale <factor> parses");
    afterhours::graphics::Config cfg;
    a.apply_to(cfg);
    check(cfg.hidpi && cfg.hidpi_scale == 2,
          "capture scale reaches a graphics Config as hidpi density");
    afterhours::graphics::RunConfig run;
    a.apply_to(run);
    check(run.capture_scale == 2, "and reaches a RunConfig");
  }
  {
    const E2EArgs a = parse({"--capture-scale=3"});
    check(a.capture_scale == 3, "--capture-scale=<factor> parses");
  }
  {
    const E2EArgs a = parse({"--capture-scale=0"});
    check(a.capture_scale == 1, "--capture-scale below 1 clamps to 1x");
  }

  {
    const E2EArgs a = parse({"--timeout=5", "--headless"});
    check(near(a.timeout_seconds, 5.0f) && a.headless,
          "an = value does not swallow the next argument");
  }
  {
    const E2EArgs a = parse({"--test-script=dir/a=b.e2e"});
    check(a.script_path == "dir/a=b.e2e",
          "only the first = splits key from value");
  }

  {
    const E2EArgs a = parse({"--bogus", "value", "--bogus=1", "--headless"});
    check(!a.enabled && a.headless && near(a.timeout_seconds, 30.0f),
          "unknown arguments stay ignored, separated and = alike");
  }
  {
    const E2EArgs a = parse({"--timeoutx=5", "--time-scale2=9"});
    check(near(a.timeout_seconds, 30.0f) && near(a.time_scale, 1.0f),
          "near-miss keys stay unknown, not prefix-matched");
  }

  check(should_run({"--test-script=a.e2e"}),
        "should_run_e2e sees --test-script=<path>");
  check(should_run({"--test-script-dir=scripts"}),
        "should_run_e2e sees --test-script-dir=<dir>");
  check(should_run({"--test-script", "a.e2e"}),
        "should_run_e2e still sees the separated form");
  check(!should_run({"--timeout=5"}),
        "should_run_e2e still ignores non-script arguments");

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
