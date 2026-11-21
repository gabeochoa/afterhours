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

  void add(Step step) { steps_.push_back(std::move(step)); }
  void on_finished(std::function<void()> fn) { on_finished_ = std::move(fn); }
  void set_progress(float p) { progress_ = std::clamp(p, 0.f, 1.f); }
  float progress() const { return progress_; }
  bool finished() const { return finished_; }
  size_t index() const { return index_; }
  const Step *current() const {
    return index_ < steps_.size() ? &steps_[index_] : nullptr;
  }

  float alpha() const {
    const Step *s = current();
    if (!s || finished_) return 0.f;
    switch (phase_) {
    case Phase::FadeIn:
      return s->fade_in > 0.f ? std::clamp(phase_t_ / s->fade_in, 0.f, 1.f) : 1.f;
    case Phase::Hold:
      return 1.f;
    case Phase::FadeOut:
      return s->fade_out > 0.f ? std::clamp(1.f - phase_t_ / s->fade_out, 0.f, 1.f) : 0.f;
    }
    return 0.f;
  }

  void skip() {
    const Step *s = current();
    if (!s || finished_ || !s->skippable) return;
    advance();
  }

  void update(float dt) {
    if (finished_) return;
    if (steps_.empty()) { finish(); return; }
    if (dt <= 0.f) return;
    phase_t_ += dt;
    while (!finished_) {
      const Step &s = steps_[index_];
      float dur = phase_ == Phase::FadeIn    ? s.fade_in
                  : phase_ == Phase::Hold    ? s.hold
                                             : s.fade_out;
      if (phase_ == Phase::Hold && s.wait_for_progress && progress_ < 1.f) {
        phase_t_ = 0.f;
        return;
      }
      if (phase_t_ < dur) return;
      phase_t_ -= dur;
      if (phase_ == Phase::FadeIn) phase_ = Phase::Hold;
      else if (phase_ == Phase::Hold) phase_ = Phase::FadeOut;
      else advance();
    }
  }

  void draw() const {
    const Step *s = current();
    if (s && !finished_ && s->draw) s->draw(alpha());
  }

private:
  enum class Phase { FadeIn, Hold, FadeOut };
  void advance() {
    index_++;
    phase_ = Phase::FadeIn;
    phase_t_ = 0.f;
    if (index_ >= steps_.size()) finish();
  }

  void finish() {
    if (finished_) return;
    finished_ = true;
    if (on_finished_) on_finished_();
  }

  std::vector<Step> steps_;
  std::function<void()> on_finished_;
  size_t index_ = 0;
  Phase phase_ = Phase::FadeIn;
  float phase_t_ = 0.f;
  float progress_ = 0.f;
  bool finished_ = false;
};

}
