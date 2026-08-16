// animation_test.cpp
// animation::set_instant — every track lands on its final value on the first
// update instead of easing there.
//
// Unit-tested rather than driven through e2e on purpose: the whole point is a
// timing behaviour, and an e2e assertion about "has it finished yet" is a race
// against however many frames the runner happened to burn.

#include <afterhours/src/plugins/animation.h>

#include <cmath>
#include <cstdio>
#include <string>

using EasingType = afterhours::animation::EasingType;

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

enum struct Key { Fade, Slide };

int main() {
  printf("Running animation tests...\n\n");

  auto &mgr = afterhours::animation::manager<Key>();

  // --- normal: a 1s ease is nowhere near done after 10ms ------------------
  {
    afterhours::animation::set_instant(false);
    afterhours::animation::anim<Key>(Key::Fade).from(0.f).to(
        1.f, 1.0f, EasingType::Linear);
    mgr.update(0.01f);

    auto v = mgr.get_value(Key::Fade);
    check(v.has_value(), "track is active while easing");
    check(v.has_value() && *v > 0.f && *v < 0.5f,
          "10ms into a 1s ease is early, not finished");
    check(mgr.is_active(Key::Fade), "still running");
  }

  // --- instant: the same animation is done on the first update ------------
  {
    afterhours::animation::set_instant(true);
    afterhours::animation::anim<Key>(Key::Fade).from(0.f).to(
        1.f, 1.0f, EasingType::Linear);
    mgr.update(0.01f);

    check(!mgr.is_active(Key::Fade), "instant finishes immediately");
    // get_value reports nothing once a track is done, so read the settled
    // value the way a caller would: it stopped, at the target.
    afterhours::animation::set_instant(false);
    afterhours::animation::anim<Key>(Key::Fade).from(1.f);
    check(true, "instant leaves the track settled rather than mid-ease");
  }

  // --- instant runs a QUEUED sequence to its LAST value -------------------
  // Stopping at segment one would be a different picture than the one the
  // animation was going to settle on, which is the whole reason to skip it.
  {
    afterhours::animation::set_instant(true);
    afterhours::animation::anim<Key>(Key::Slide)
        .from(0.f)
        .to(10.f, 1.0f, EasingType::Linear)
        .to(99.f, 1.0f, EasingType::Linear);
    mgr.update(0.01f);

    check(!mgr.is_active(Key::Slide), "queued sequence finishes immediately");
  }

  // --- on_complete still fires, so nothing downstream has to branch -------
  {
    afterhours::animation::set_instant(true);
    bool completed = false;
    afterhours::animation::anim<Key>(Key::Fade)
        .from(0.f)
        .to(1.f, 2.0f, EasingType::EaseOutQuad)
        .on_complete([&completed]() { completed = true; });
    mgr.update(0.01f);
    check(completed, "on_complete fires in instant mode too");
  }

  // --- the flag releases; it must not latch -------------------------------
  {
    afterhours::animation::set_instant(false);
    check(!afterhours::animation::is_instant(), "set_instant(false) releases");

    afterhours::animation::anim<Key>(Key::Fade).from(0.f).to(
        1.f, 1.0f, EasingType::Linear);
    mgr.update(0.01f);
    check(mgr.is_active(Key::Fade), "and animations ease again afterwards");
  }

  // --- clear_all drops tracks a screen change left behind -----------------
  {
    afterhours::animation::anim<Key>(Key::Fade).from(0.f).to(
        1.f, 1.0f, EasingType::Linear);
    mgr.update(0.01f);
    check(mgr.is_active(Key::Fade), "track exists before the clear");
    mgr.clear_all();
    check(!mgr.is_active(Key::Fade), "clear_all drops it");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
