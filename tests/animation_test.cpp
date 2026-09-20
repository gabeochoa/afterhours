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

  using Timeline = afterhours::animation::Timeline;
  using Repeat = Timeline::Repeat;
  auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };

  {
    const Timeline shake{.keys = {{0.f, 0.f},
                                  {.08f, 6.f},
                                  {.16f, -6.f},
                                  {.22f, 4.f},
                                  {.28f, 0.f}}};
    check(near(shake.at(0.f), 0.f) && near(shake.at(.08f), 6.f) &&
              near(shake.at(.16f), -6.f) && near(shake.at(.22f), 4.f) &&
              near(shake.at(.28f), 0.f),
          "timeline hits every key exactly");
    check(near(shake.at(.04f), 3.f) && near(shake.at(.12f), 0.f),
          "timeline is linear between keys");
    check(near(shake.at(-1.f), 0.f) && near(shake.at(5.f), 0.f),
          "timeline clamps before the first and after the last key");
    check(!shake.finished(.27f) && shake.finished(.28f),
          "once finishes at its length");
    check(near(shake.length(), .28f), "length is the last key");
  }

  {
    const Timeline spin{.keys = {{0.f, 0.f}, {0.9f, 360.f}},
                        .repeat = Repeat::Loop};
    check(near(spin.at(0.45f), 180.f) && near(spin.at(1.35f), 180.f) &&
              near(spin.at(9.45f), 180.f),
          "loop wraps every cycle");
    check(!spin.finished(100.f), "loop never finishes");
  }

  {
    const Timeline pulse{.keys = {{0.f, 1.f}, {0.5f, 0.5f}},
                         .repeat = Repeat::PingPong};
    check(near(pulse.at(0.25f), 0.75f) && near(pulse.at(0.5f), 0.5f) &&
              near(pulse.at(0.75f), 0.75f) && near(pulse.at(1.f), 1.f) &&
              near(pulse.at(1.25f), 0.75f),
          "pingpong runs forward then back");
  }

  {
    Timeline eased{.keys = {{0.f, 0.f}, {1.f, 10.f}}};
    eased.curve = [](float u) { return u * u; };
    check(near(eased.at(0.5f), 2.5f), "curve applies per segment");
    check(near(Timeline{}.at(3.f), 0.f), "empty timeline reads 0");
    check(near(Timeline{.keys = {{0.f, 7.f}}}.at(3.f), 7.f),
          "single key holds its value");
  }

  auto run = [](auto &track, float total, float dt = 1.f / 60.f) {
    for (float t = 0.f; t < total - 1e-6f; t += dt)
      track.advance(std::min(dt, total - t));
  };

  {
    afterhours::motion::Track<float> tr;
    tr.from(0.f).to(100.f, Spring{.response = 0.3f});
    check(tr.active(), "to() starts the track");
    run(tr, 0.1f);
    const float mid = tr.value();
    check(mid > 0.f && mid < 100.f, "mid-flight value is between");
    run(tr, 2.f);
    check(!tr.active() && near(tr.value(), 100.f),
          "float track settles on its target and deactivates");
  }

  {
    afterhours::motion::Track<Vector2Type> tr;
    tr.from({0.f, 0.f}).to({10.f, -20.f}, Spring{});
    run(tr, 2.f);
    check(near(tr.value().x, 10.f) && near(tr.value().y, -20.f),
          "vector track lands on both components");
    afterhours::motion::Track<RectangleType> rt;
    rt.from({0.f, 0.f, 100.f, 50.f}).to({10.f, 10.f, 200.f, 80.f}, Spring{});
    run(rt, 2.f);
    check(near(rt.value().width, 200.f) && near(rt.value().height, 80.f),
          "rectangle track lands on size");
    afterhours::motion::Track<ColorType> ct;
    ct.from(ColorType{0, 0, 0, 255}).to(ColorType{200, 100, 50, 255},
                                         Spring{});
    run(ct, 2.f);
    check(ct.value().r == 200 && ct.value().g == 100 && ct.value().b == 50 &&
              ct.value().a == 255,
          "color track lands on every channel");
  }

  {
    afterhours::motion::Track<float> tr;
    tr.from(0.f).to(100.f, Spring{.response = 0.3f}).delay(0.2f);
    run(tr, 0.19f);
    check(near(tr.value(), 0.f), "delay holds the start value");
    run(tr, 0.1f);
    check(tr.value() > 0.f, "motion begins after the delay");
  }

  {
    afterhours::motion::Track<float> tr;
    int completions = 0;
    tr.from(0.f)
        .to(10.f, Spring{.response = 0.2f})
        .then(20.f, Spring{.response = 0.2f})
        .then(0.f, Spring{.response = 0.2f})
        .on_complete([&] { completions++; });
    check(near(tr.target(), 0.f), "target() reports the end of the chain");
    run(tr, 0.6f);
    check(tr.active() && tr.value() > 10.f,
          "chain is on a later step after the first settles");
    run(tr, 3.f);
    check(!tr.active() && near(tr.value(), 0.f) && completions == 1,
          "chain finishes at its last target and completes once");
  }

  {
    afterhours::motion::Track<float> tr;
    tr.from(0.f).to(100.f, Spring{.response = 0.3f}).then(200.f, Spring{});
    run(tr, 0.1f);
    const float before = tr.value();
    tr.to(0.f, Spring{.response = 0.3f});
    check(near(tr.value(), before) && near(tr.target(), 0.f),
          "to() mid-flight keeps the current value and drops the queue");
    tr.advance(0.01f);
    check(tr.value() > before,
          "momentum carries through the interrupt before reversing");
  }

  {
    afterhours::motion::Track<float> tr;
    tr.from(0.f).to(6.f, Timeline{.keys = {{0.f, 0.f},
                                           {.08f, 1.f},
                                           {.16f, -1.f},
                                           {.22f, .667f},
                                           {.28f, 0.f}}});
    run(tr, 0.08f);
    check(near(tr.value(), 6.f), "timeline progress scales the target");
    run(tr, 0.08f);
    check(near(tr.value(), -6.f), "timeline can overshoot below the start");
    run(tr, 0.2f);
    check(!tr.active() && near(tr.value(), 0.f),
          "once-timeline finishes at its length");
  }

  {
    afterhours::motion::Track<ColorType> tr;
    tr.from(ColorType{0, 0, 0, 255})
        .to(ColorType{255, 255, 255, 255},
            Timeline{.keys = {{0.f, 0.f}, {0.5f, 1.f}},
                     .repeat = Timeline::Repeat::PingPong});
    run(tr, 0.5f);
    check(tr.value().r == 255, "looping color timeline reaches its peak");
    run(tr, 0.5f);
    check(tr.value().r == 0 && tr.active(),
          "pingpong color timeline returns and stays active");
  }

  {
    afterhours::motion::Track<float> tr;
    tr.from(0.f)
        .to(1.f, Spring{.response = 0.2f})
        .then(0.f, Spring{.response = 0.2f})
        .repeat();
    run(tr, 5.f);
    check(tr.active(), "repeat() keeps a chain alive");
  }

  {
    afterhours::motion::Track<float> tr;
    const Timeline tenth{.keys = {{0.f, 0.f}, {0.1f, 1.f}}};
    tr.from(0.f).to(1.f, tenth).then(2.f, tenth);
    tr.advance(0.15f);
    check(tr.active() && near(tr.value(), 1.5f),
          "time left over when a step lands carries into the next step");
    afterhours::motion::Track<float> looped;
    looped.from(0.f).to(1.f, tenth).then(0.f, tenth).repeat();
    looped.advance(0.25f);
    check(near(looped.value(), 0.5f), "carry-over also crosses a repeat boundary");
  }

  {
    afterhours::motion::Track<float> tr;
    tr.from(1.f).to(1.f).repeat();
    tr.advance(0.016f);
    check(near(tr.value(), 1.f), "repeating an already-settled spring returns");
  }

  {
    afterhours::motion::Track<float> tr;
    std::vector<int> steps;
    tr.from(0.f)
        .to(10.f, Spring{})
        .on_step(2.5f, [&](int s) { steps.push_back(s); });
    run(tr, 2.f);
    check(steps.size() >= 4 && steps.back() == 4,
          "on_step fires for each quantised step and ends on the last");
  }

  {
    afterhours::animation::set_instant(true);
    afterhours::motion::Track<float> tr;
    bool done = false;
    tr.from(0.f)
        .to(10.f, Spring{})
        .then(50.f, Spring{})
        .delay(5.f)
        .on_complete([&] { done = true; });
    tr.advance(0.001f);
    check(!tr.active() && near(tr.value(), 50.f) && done,
          "instant jumps to the chain's last target, skipping delays, and "
          "still completes");
    afterhours::animation::set_instant(false);
  }

  namespace motion = afterhours::motion;
  using afterhours::Entity;
  using afterhours::EntityHelper;
  enum struct Prop { Slide, Tint };

  {
    afterhours::SystemManager sm;
    motion::register_update_systems(sm);

    Entity &e = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    const auto id = e.id;

    motion::anim(Prop::Slide, id).from(0.f).to(100.f, Spring{});
    motion::anim<ColorType>(Prop::Tint, id)
        .from(ColorType{0, 0, 0, 255})
        .to(ColorType{255, 0, 0, 255}, Spring{});
    check(e.has<motion::HasTracks>(), "anim(key, id) stores on the entity");
    check(motion::anim(Prop::Slide, id).value_or(-1.f) == 0.f,
          "value_or reads a started track");
    check(motion::anim(Prop::Tint).value_or(-1.f) == -1.f,
          "value_or falls back for a track nobody started");

    for (int i = 0; i < 120; ++i)
      sm.run(1.f / 60.f);
    check(near(motion::anim(Prop::Slide, id).value(), 100.f) &&
              motion::anim<ColorType>(Prop::Tint, id).value().r == 255,
          "the registered system advances every track on the entity");

    motion::anim(Prop::Slide).from(0.f).to(1.f, Spring{});
    check(EntityHelper::has_singleton<motion::MotionRoot>(),
          "keyless anim lives on a hidden root entity");
    for (int i = 0; i < 120; ++i)
      sm.run(1.f / 60.f);
    check(near(motion::anim(Prop::Slide).value(), 1.f),
          "root tracks advance too");

    e.cleanup = true;
    EntityHelper::cleanup();
    check(!EntityHelper::getEntityForID(id).has_value(),
          "entity is gone after cleanup");
    Entity &again = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    check(!motion::anim(Prop::Slide, again.id).started(),
          "a fresh entity inherits no tracks");
  }

  {
    afterhours::SystemManager sm;
    motion::register_update_systems(sm);
    Entity &e = EntityHelper::createEntity();
    EntityHelper::merge_entity_arrays();
    motion::anim(Prop::Slide, e.id).from(0.f).to(100.f, Spring{});

    motion::pause(true);
    for (int i = 0; i < 30; ++i)
      sm.run(1.f / 60.f);
    check(near(motion::anim(Prop::Slide, e.id).value(), 0.f),
          "pause freezes progress");
    motion::pause(false);

    motion::set_time_scale(0.f);
    for (int i = 0; i < 30; ++i)
      sm.run(1.f / 60.f);
    check(near(motion::anim(Prop::Slide, e.id).value(), 0.f),
          "time scale 0 freezes progress");

    motion::set_time_scale(4.f);
    sm.run(1.f / 60.f);
    const float fast = motion::anim(Prop::Slide, e.id).value();
    motion::set_time_scale(1.f);
    motion::anim(Prop::Slide, e.id).from(0.f).to(100.f, Spring{});
    for (int i = 0; i < 4; ++i)
      sm.run(1.f / 60.f);
    check(near(fast, motion::anim(Prop::Slide, e.id).value()),
          "time scale 4 covers four frames in one");
    e.cleanup = true;
    EntityHelper::cleanup();
  }

  {
    afterhours::animation::set_instant(true);
    motion::Track<float> spinner;
    spinner.from(0.f)
        .to(360.f, Timeline{.keys = {{0.f, 0.f}, {0.9f, 1.f}},
                            .repeat = Timeline::Repeat::Loop})
        .essential();
    run(spinner, 0.45f);
    check(spinner.active() && near(spinner.value(), 180.f),
          "essential loops keep running under instant");
    motion::Track<float> shimmer;
    shimmer.from(0.f).to(1.f, Timeline{.keys = {{0.f, 0.f}, {2.f, 1.f}},
                                       .repeat = Timeline::Repeat::Loop});
    shimmer.advance(0.01f);
    check(!shimmer.active(), "non-essential loops stop under instant");
    afterhours::animation::set_instant(false);
  }

  printf("\n%d/%d checks passed\n", checks_passed, checks_run);
  if (checks_passed != checks_run) {
    printf("FAILURES: %d\n", checks_run - checks_passed);
    return 1;
  }
  printf("All checks passed!\n");
  return 0;
}
