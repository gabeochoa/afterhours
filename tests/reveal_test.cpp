
#include <afterhours/src/plugins/animation/reveal.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }

int main() {
  {
    motion::Reveal r{.duration = 0.4f};
    check(near(r.update(0.1f, true), 0.25f), "quarter open");
    check(near(r.update(0.1f, true), 0.5f), "half open");
    check(!r.settled(true), "not settled mid-flight");
    check(near(r.update(0.5f, true), 1.f), "overshoot clamps at 1");
    check(r.settled(true) && near(r.eased(), 1.f), "settled open, eased is exact");
    check(near(r.update(0.1f, true), 1.f), "stays at 1");
  }

  {
    motion::Reveal r{.duration = 0.4f};
    r.update(0.2f, true);
    check(near(r.update(0.1f, false), 0.25f), "reverse from half");
    check(near(r.update(0.5f, false), 0.f), "overshoot clamps at 0");
    check(r.settled(false) && near(r.eased(), 0.f), "settled closed, eased is exact");
  }

  {
    motion::Reveal r{.duration = 0.4f};
    check(near(r.update(0.f, true), 0.f), "zero dt no change");
    check(near(r.update(-1.f, true), 0.f), "negative dt no change");
    motion::Reveal no_dur{.duration = 0.f};
    check(near(no_dur.update(0.001f, true), 1.f), "zero duration snaps open");
    check(near(no_dur.update(0.001f, false), 0.f), "zero duration snaps closed");
  }

  {
    motion::Reveal r{.duration = 5.f};
    motion::set_instant(true);
    check(near(r.update(0.001f, true), 1.f), "instant snaps open");
    check(near(r.update(0.001f, false), 0.f), "instant snaps closed");
    motion::set_instant(false);
  }

  {
    motion::Reveal r{.duration = 1.f, .curve = &motion::curves::ease_out_quad};
    float prev = -1.f;
    bool mono = true;
    for (int i = 0; i < 10; ++i) {
      r.update(0.1f, true);
      const float e = r.eased();
      if (e < prev - 1e-6f || e < 0.f || e > 1.f)
        mono = false;
      prev = e;
    }
    check(mono && near(r.eased(), 1.f), "ease_out_quad monotonic to 1");
    const float mid = [] {
      motion::Reveal q{.duration = 1.f, .curve = &motion::curves::ease_out_quad};
      q.update(0.5f, true);
      return q.eased();
    }();
    check(mid > 0.5f && mid < 1.f, "ease_out mid ahead of linear");
  }

  {
    motion::Reveal r{};
    r.snap(true);
    check(r.settled(true), "snap open");
    r.snap(false);
    check(r.settled(false), "snap closed");
  }

  if (failures == 0) {
    std::printf("reveal_test: all %d checks passed\n", checks);
    return 0;
  }
  std::printf("reveal_test: %d/%d checks failed\n", failures, checks);
  return 1;
}
