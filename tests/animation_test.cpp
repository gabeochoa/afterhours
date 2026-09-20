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

  {
    constexpr EasingType easings[] = {
        EasingType::Linear, EasingType::EaseOutQuad, EasingType::Hold};
    const auto midpoint = [](EasingType easing) {
      switch (easing) {
      case EasingType::Linear: return 0.5f;
      case EasingType::EaseOutQuad: return 0.75f;
      case EasingType::Hold: return 0.f;
      }
      return -1.f;
    };
    for (auto first : easings) {
      for (auto second : easings) {
        mgr.clear_all();
        for (int replay = 0; replay < 2; ++replay) {
          afterhours::animation::anim<Key>(Key::Fade).from(0.f).sequence(
              {{10.f, .5f, first}, {20.f, .5f, second}});
          mgr.update(.25f);
          check(std::abs(mgr.ensure_track(Key::Fade).current -
                         10.f * midpoint(first)) < .00001f,
                "fresh and replayed sequences use the first easing");
          mgr.update(.25f);
          check(mgr.ensure_track(Key::Fade).current == 10.f,
                "first segment reaches its endpoint");
          mgr.update(.25f);
          check(std::abs(mgr.ensure_track(Key::Fade).current -
                         (10.f + 10.f * midpoint(second))) < .00001f,
                "queued segment uses its own easing");
          mgr.update(.25f);
          check(!mgr.is_active(Key::Fade) &&
                    mgr.ensure_track(Key::Fade).current == 20.f,
                "sequence finishes at its endpoint");
        }
      }
    }
  }

  {
    mgr.clear_all();
    afterhours::animation::anim<Key>(Key::Fade).from(0.f).sequence(
        {{1.15f, .6f, EasingType::EaseOutQuad},
         {1.f, .4f, EasingType::EaseOutQuad}});
    mgr.update(.5f);
    check(std::abs(mgr.ensure_track(Key::Fade).current - 1.1180556f) < .00001f,
          "WM scale sequence matches its half-second preview");
  }

  {
    mgr.clear_all();
    afterhours::animation::anim<Key>(Key::Fade).from(0.f)
        .to(10.f, .5f, EasingType::Hold);
    mgr.update(.5f);
    afterhours::animation::anim<Key>(Key::Fade).sequence(
        {{0.f, .5f, EasingType::EaseOutQuad}});
    mgr.update(.25f);
    check(std::abs(mgr.ensure_track(Key::Fade).current - 2.5f) < .00001f,
          "sequence replaces an idle track's previous easing without from");
  }

  {
    mgr.clear_all();
    afterhours::animation::anim<Key>(Key::Fade).from(0.f)
        .to(10.f, .5f, EasingType::Linear)
        .sequence({{20.f, .5f, EasingType::EaseOutQuad}});
    mgr.update(.25f);
    check(mgr.ensure_track(Key::Fade).current == 5.f,
          "appending a sequence preserves the active segment's easing");
    mgr.update(.25f);
    mgr.update(.25f);
    check(mgr.ensure_track(Key::Fade).current == 17.5f,
          "appended sequence starts with its own easing");
  }

  {
    mgr.clear_all();
    afterhours::animation::anim<Key>(Key::Fade).from(0.f).loop_sequence(
        {{10.f, .5f, EasingType::EaseOutQuad},
         {0.f, .5f, EasingType::Hold}});
    for (int cycle = 0; cycle < 3; ++cycle) {
      mgr.update(.25f);
      check(mgr.ensure_track(Key::Fade).current == 7.5f,
            "loop restores first easing after a different final easing");
      mgr.update(.25f);
      mgr.update(.5f);
      check(mgr.is_active(Key::Fade) &&
                mgr.ensure_track(Key::Fade).current == 0.f,
            "loop restarts from its final value");
    }
    mgr.clear_all();
  }

  using Spring = afterhours::animation::Spring;
  using SpringState = afterhours::animation::SpringState;
  auto spring_solve = [](const Spring &s, const SpringState &st, float t) {
    return afterhours::animation::spring_solve(s, st, t);
  };
  auto spring_settle_time = [](const Spring &s, const SpringState &st) {
    return afterhours::animation::spring_settle_time(s, st);
  };

  auto step_to = [&](const Spring &s, SpringState st, float total, float dt) {
    afterhours::animation::SpringSample out{st.x0, st.v0};
    for (float t = 0.f; t < total - 1e-6f; t += dt) {
      out = spring_solve(s, st, std::min(dt, total - t));
      st.x0 = out.x;
      st.v0 = out.v;
    }
    return out;
  };

  {
    const Spring s{.response = 0.3f, .bounce = 0.2f};
    const SpringState st{.x0 = 0.f, .v0 = 0.f, .target = 100.f};
    const auto direct = spring_solve(s, st, 0.25f);
    const auto at30 = step_to(s, st, 0.25f, 1.f / 30.f);
    const auto at60 = step_to(s, st, 0.25f, 1.f / 60.f);
    const auto at144 = step_to(s, st, 0.25f, 1.f / 144.f);
    const auto irregular = [&] {
      SpringState cur = st;
      afterhours::animation::SpringSample out{};
      for (float dt : {0.001f, 0.05f, 0.0163f, 0.1f, 0.0827f}) {
        out = spring_solve(s, cur, dt);
        cur.x0 = out.x;
        cur.v0 = out.v;
      }
      return out;
    }();
    check(std::fabs(at30.x - direct.x) < 1e-2f &&
              std::fabs(at60.x - direct.x) < 1e-2f &&
              std::fabs(at144.x - direct.x) < 1e-2f,
          "re-solving from the current sample each frame lands where one "
          "solve does at 30/60/144 fps");
    check(std::fabs(irregular.x - direct.x) < 1e-2f,
          "irregular frame times land at the same place");
  }

  {
    const Spring s{.response = 0.3f, .bounce = 0.3f};
    SpringState st{.x0 = 0.f, .v0 = 0.f, .target = 100.f};
    const auto before = spring_solve(s, st, 0.05f);
    st.x0 = before.x;
    st.v0 = before.v;
    st.target = 0.f;
    const auto after = spring_solve(s, st, 0.f);
    check(std::fabs(after.x - before.x) < 1e-3f &&
              std::fabs(after.v - before.v) < 1e-2f,
          "retarget keeps position and velocity");
    const auto later = spring_solve(s, st, 0.02f);
    check(later.x > before.x,
          "momentum carries past the reversal instead of snapping back");
  }

  {
    const Spring s{.response = 0.3f, .bounce = 0.4f};
    SpringState st{.x0 = 0.f, .v0 = 0.f, .target = 100.f};
    for (int i = 0; i < 6; ++i) {
      const auto smp = spring_solve(s, st, 0.03f);
      st.x0 = smp.x;
      st.v0 = smp.v;
      st.target = (i % 2 == 0) ? 0.f : 100.f;
    }
    const float settle = spring_settle_time(s, st);
    const float w = afterhours::animation::spring_omega(s);
    bool residual_ok = true;
    for (float t = settle; t < settle + 2.f; t += 0.01f) {
      const auto smp = spring_solve(s, st, t);
      if (std::fabs(smp.x - st.target) > 0.1f || std::fabs(smp.v) > 0.1f * w)
        residual_ok = false;
    }
    check(residual_ok,
          "after rapid reversals the snap at settle time hides a residual "
          "under 0.1% of the motion for every later t");
    check(settle > 0.f && settle < 3.f, "settle time is finite and sane");
  }

  {
    const Spring s{};
    const SpringState st{.x0 = 0.f, .v0 = 0.f, .target = 40.f};
    const auto stalled = spring_solve(s, st, 2.f);
    check(std::fabs(stalled.x - 40.f) < 1e-3f,
          "a 2s stalled frame lands on the target instead of slowing down");
    check(spring_settle_time(s, st) < 2.f, "default spring settles within 2s");
  }

  {
    const SpringState st{.x0 = 0.f, .v0 = 0.f, .target = 1.f};
    bool crossed = false, overshot = false;
    for (float t = 0.f; t < 2.f; t += 0.005f) {
      if (spring_solve(Spring{.bounce = 0.f}, st, t).x > 1.f + 1e-4f)
        crossed = true;
      if (spring_solve(Spring{.bounce = 0.5f}, st, t).x > 1.05f)
        overshot = true;
    }
    check(!crossed, "bounce 0 never crosses the target");
    check(overshot, "bounce 0.5 overshoots");
  }

  {
    const Spring s{};
    check(spring_settle_time(s, SpringState{.x0 = 5.f, .target = 5.f}) == 0.f,
          "already at rest settles immediately");
    const float small = spring_settle_time(
        s, SpringState{.x0 = 0.f, .v0 = 0.f, .target = 1.f});
    const float large = spring_settle_time(
        s, SpringState{.x0 = 0.f, .v0 = 0.f, .target = 400.f});
    check(std::fabs(small - large) < 1e-3f,
          "default tolerances scale with the motion, so settle time does not "
          "depend on distance");
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
