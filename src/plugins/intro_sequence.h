#pragma once


#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace afterhours {

struct IntroSequence {
  struct Step {
    float fade_in = 0.25f;
    float hold = 1.5f;
    float fade_out = 0.25f;
    bool skippable = true;
    bool wait_for_progress = false;
    std::function<void(float alpha)> draw;
  };

  void add(Step step) { steps.push_back(std::move(step)); }
  void on_finished(std::function<void()> fn) { on_finished_fn = std::move(fn); }
  void set_progress(float p) { progress_value = std::clamp(p, 0.f, 1.f); }
  float progress() const { return progress_value; }
  bool finished() const { return is_finished; }
  size_t index() const { return current_step; }
  const Step *current() const {
    return current_step < steps.size() ? &steps[current_step] : nullptr;
  }

  float alpha() const {
    const Step *s = current();
    if (!s || is_finished) return 0.f;
    switch (current_phase) {
    case Phase::FadeIn:
      return s->fade_in > 0.f ? std::clamp(phase_time / s->fade_in, 0.f, 1.f) : 1.f;
    case Phase::Hold:
      return 1.f;
    case Phase::FadeOut:
      return s->fade_out > 0.f ? std::clamp(1.f - phase_time / s->fade_out, 0.f, 1.f) : 0.f;
    }
    return 0.f;
  }

  void skip() {
    const Step *s = current();
    if (!s || is_finished || !s->skippable) return;
    advance();
  }

  void update(float dt) {
    if (is_finished) return;
    if (steps.empty()) { finish(); return; }
    if (dt <= 0.f) return;
    phase_time += dt;
    while (!is_finished) {
      const Step &s = steps[current_step];
      float dur = current_phase == Phase::FadeIn    ? s.fade_in
                  : current_phase == Phase::Hold    ? s.hold
                                             : s.fade_out;
      if (current_phase == Phase::Hold && s.wait_for_progress && progress_value < 1.f) {
        phase_time = 0.f;
        return;
      }
      if (phase_time < dur) return;
      phase_time -= dur;
      if (current_phase == Phase::FadeIn) current_phase = Phase::Hold;
      else if (current_phase == Phase::Hold) current_phase = Phase::FadeOut;
      else advance();
    }
  }

  void draw() const {
    const Step *s = current();
    if (s && !is_finished && s->draw) s->draw(alpha());
  }

private:
  enum class Phase { FadeIn, Hold, FadeOut };
  void advance() {
    current_step++;
    current_phase = Phase::FadeIn;
    phase_time = 0.f;
    if (current_step >= steps.size()) finish();
  }

  void finish() {
    if (is_finished) return;
    is_finished = true;
    if (on_finished_fn) on_finished_fn();
  }

  std::vector<Step> steps;
  std::function<void()> on_finished_fn;
  size_t current_step = 0;
  Phase current_phase = Phase::FadeIn;
  float phase_time = 0.f;
  float progress_value = 0.f;
  bool is_finished = false;
};

}
