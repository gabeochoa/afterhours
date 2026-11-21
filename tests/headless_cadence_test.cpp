#include <afterhours/src/frame_loop.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

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

static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

int main() {
  printf("Running headless_cadence tests...\n\n");
  using namespace afterhours::frame_loop;

  {
    CadenceDriver driver(Mode::Continuous, 0);
    std::vector<float> dts;
    const double callbacks[] = {0.001, 0.5, 0.016};
    const auto stats = drive_headless_cadence(
        driver, callbacks, {}, [&](float dt) { dts.push_back(dt); });
    check(stats.callbacks == 3 && stats.admitted == 3 && stats.skipped == 0,
          "uncapped continuous admits every callback");
    check(dts.size() == 3 && near(dts[1], 0.5),
          "uncapped admitted dt is the callback dt");
  }

  {
    CadenceDriver driver(Mode::Continuous, 60);
    std::vector<float> dts;
    const double callbacks[] = {0.010, 0.010, 0.010, 0.010, 0.010, 0.010};
    const auto stats = drive_headless_cadence(
        driver, callbacks, {}, [&](float dt) { dts.push_back(dt); });
    check(stats.callbacks == 6 && stats.admitted == 3 && stats.skipped == 3,
          "60fps cap admits every second 10ms callback");
    check(dts.size() == 3 && near(dts[0], 0.020) && near(dts[2], 0.020),
          "admitted dt accumulates skipped callback time");
  }

  {
    CadenceDriver driver(Mode::Continuous, 60);
    int frames = 0;
    std::vector<double> callbacks(1200, 0.010);
    const auto stats = drive_headless_cadence(
        driver, callbacks, {}, [&](float) { frames++; });
    check(stats.admitted == 600 && stats.skipped == 600 && frames == 600,
          "long scripted run admits at the capped cadence");
  }

  {
    CadenceDriver driver(Mode::OnDemand, 60);
    std::vector<float> dts;
    const double callbacks[] = {0.016, 0.016, 0.016, 0.016};
    const bool requested[] = {false, true, false, true};
    const auto stats = drive_headless_cadence(
        driver, callbacks, requested, [&](float dt) { dts.push_back(dt); });
    check(stats.admitted == 2 && stats.skipped == 2,
          "on-demand admits only requested callbacks");
    check(dts.size() == 2 && near(dts[0], 0.032) && near(dts[1], 0.032),
          "on-demand admitted dt spans the idle callbacks");
  }

  {
    CadenceDriver driver(Mode::Continuous, 60);
    driver.prime();
    const auto first = driver.on_callback(0.001);
    check(first.admitted, "primed driver admits the first callback");
    const auto second = driver.on_callback(0.001);
    check(!second.admitted, "priming does not admit the second callback");
  }

  {
    CadenceDriver driver(Mode::Continuous, 60);
    check(!driver.on_callback(-1.0).admitted &&
              driver.on_callback(0.020).admitted,
          "negative callback dt is ignored, not credited");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
