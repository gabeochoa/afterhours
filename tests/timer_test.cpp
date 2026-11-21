
#define AFTER_HOURS_USE_NONE
#include <afterhours/src/plugins/timer.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

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

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
  printf("Running timer tests...\n\n");
  using HasTimer = timer::HasTimer;

  {
    HasTimer t(10.f);
    check(!t.is_ready(), "count-up not ready at 0");
    t.update(3.f);
    check(approx(t.current_time, 3.f), "count-up accumulates");
    check(approx(t.get_remaining(), 7.f), "count-up remaining");
    check(approx(t.get_progress(), 0.3f), "count-up progress");
    check(!t.update(3.f), "count-up not fired yet");
    check(t.update(5.f), "count-up fires at reset_time");
  }

  {
    HasTimer t = HasTimer::make_countdown(10.f);
    check(t.countdown, "countdown flag set");
    check(approx(t.get_remaining(), 10.f), "countdown starts full");
    check(!t.is_ready(), "countdown not ready at start");
    check(!t.update(3.f), "countdown no fire early");
    check(approx(t.get_remaining(), 7.f), "countdown remaining decreases");
    check(approx(t.get_progress(), 0.3f), "countdown progress advances");
    check(t.update(7.f), "countdown fires at zero");
    check(t.is_ready(), "countdown ready at zero");
    check(approx(t.get_remaining(), 0.f), "remaining clamps at zero");
    check(t.update(1.f), "countdown stays firing once expired");
  }

  {
    HasTimer t = HasTimer::make_countdown(5.f, true);
    check(t.update(5.f), "auto countdown fires at zero");
    check(approx(t.get_remaining(), 5.f), "auto countdown refills");
    check(!t.is_ready(), "auto countdown not immediately ready again");
  }

  {
    HasTimer t = HasTimer::make_countdown(8.f);
    t.pause();
    check(!t.update(2.f), "paused countdown does not tick");
    check(approx(t.get_remaining(), 8.f), "paused remaining held");
    t.resume();
    check(!t.update(2.f), "resumed countdown ticks");
    check(approx(t.get_remaining(), 6.f), "resumed remaining down");
    t.reset();
    check(approx(t.get_remaining(), 8.f), "countdown reset refills");
    t.set_time(2.f);
    check(approx(t.get_remaining(), 2.f), "countdown set_time sets remaining");
    t.add_time(1.f);
    check(approx(t.get_remaining(), 1.f), "countdown add_time subtracts");
  }

  {
    HasTimer t = HasTimer::make_countdown(12.f, true);
    t.pause();
    t.set_time(4.5f);
    const HasTimer::HasTimerDTO dto = t.to_dto();
    HasTimer restored = HasTimer::from_dto(dto);
    check(restored.countdown, "dto keeps countdown");
    check(restored.auto_reset, "dto keeps auto_reset");
    check(restored.paused, "dto keeps paused");
    check(approx(restored.reset_time, 12.f), "dto keeps duration");
    check(approx(restored.current_time, 4.5f), "dto keeps remaining");
    check(approx(restored.get_remaining(), 4.5f), "restored remaining read");
    check(restored.is_ready() == t.is_ready(), "restored ready matches");
  }

  {
    HasTimer t(6.f, true);
    t.current_time = 2.25f;
    HasTimer restored = HasTimer::from_dto(t.to_dto());
    check(!restored.countdown, "dto count-up stays count-up");
    check(approx(restored.current_time, 2.25f), "dto count-up elapsed kept");
    check(approx(restored.get_remaining(), 3.75f), "restored count-up remaining");
  }

  printf("timer_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
