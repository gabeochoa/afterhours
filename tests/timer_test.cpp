#include <afterhours/src/plugins/timer.h>

#include <cmath>
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

static bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }

int main() {
  printf("Running timer tests...\n\n");

  using HasTimer = afterhours::timer::HasTimer;
  using TriggerOnDt = afterhours::timer::TriggerOnDt;
  auto next_deadline = [](afterhours::EntityCollection &ec) {
    return afterhours::timer::next_deadline(ec);
  };

  { // no timers -> empty
    afterhours::EntityCollection ec;
    ec.cleanup();
    check(!next_deadline(ec).has_value(), "no timers -> no deadline");
  }

  { // multiple timers -> earliest returned
    afterhours::EntityCollection ec;
    ec.createEntity().addComponent<HasTimer>(5.f);
    ec.createEntity().addComponent<HasTimer>(2.f);
    ec.createEntity().addComponent<HasTimer>(9.f);
    ec.cleanup();
    const auto d = next_deadline(ec);
    check(d && near(*d, 2.f), "earliest of several timers returned");
  }

  { // a fired timer drops out; the next earliest takes over
    afterhours::EntityCollection ec;
    auto &early = ec.createEntity();
    early.addComponent<HasTimer>(1.f);
    ec.createEntity().addComponent<HasTimer>(4.f);
    check(early.get<HasTimer>().update(2.f),
          "timer fires once past its reset time");
    ec.cleanup();
    const auto d = next_deadline(ec);
    check(d && near(*d, 4.f), "fired timer drops out of the query");
  }

  { // auto-reset: fired timer's next deadline is a full period out
    afterhours::EntityCollection ec;
    auto &t = ec.createEntity();
    t.addComponent<HasTimer>(3.f, true);
    t.get<HasTimer>().update(3.f);
    ec.cleanup();
    const auto d = next_deadline(ec);
    check(d && near(*d, 3.f), "auto-reset timer reports a full period");
  }

  { // a removed timer drops out
    afterhours::EntityCollection ec;
    auto &early = ec.createEntity();
    early.addComponent<HasTimer>(1.f);
    ec.createEntity().addComponent<HasTimer>(7.f);
    early.removeComponent<HasTimer>();
    ec.cleanup();
    const auto d = next_deadline(ec);
    check(d && near(*d, 7.f), "removed timer drops out of the query");
  }

  { // a paused timer counts as nothing pending
    afterhours::EntityCollection ec;
    auto &p = ec.createEntity();
    p.addComponent<HasTimer>(1.f);
    p.get<HasTimer>().pause();
    ec.cleanup();
    check(!next_deadline(ec).has_value(), "paused timer -> no deadline");
  }

  { // TriggerOnDt participates; partially elapsed triggers count down
    afterhours::EntityCollection ec;
    auto &t = ec.createEntity();
    t.addComponent<TriggerOnDt>(10.f);
    ec.createEntity().addComponent<HasTimer>(6.f);
    t.get<TriggerOnDt>().current = 8.f;
    ec.cleanup();
    const auto d = next_deadline(ec);
    check(d && near(*d, 2.f), "earliest across both timer kinds returned");
  }

  { // only a fired timer left -> empty again
    afterhours::EntityCollection ec;
    auto &t = ec.createEntity();
    t.addComponent<HasTimer>(1.f);
    t.get<HasTimer>().update(5.f);
    ec.cleanup();
    check(!next_deadline(ec).has_value(), "all timers fired -> no deadline");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  return 0;
}
