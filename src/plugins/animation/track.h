#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

#include "../../developer.h"
#include "spring.h"
#include "timeline.h"

namespace afterhours {
namespace motion {

template <typename T> struct Components;
template <> struct Components<float> {
  static constexpr size_t N = 1;
  static std::array<float, 1> to(float v) { return {v}; }
  static float from(const std::array<float, 1> &a) { return a[0]; }
};
template <> struct Components<Vector2Type> {
  static constexpr size_t N = 2;
  static std::array<float, 2> to(Vector2Type v) { return {v.x, v.y}; }
  static Vector2Type from(const std::array<float, 2> &a) {
    return Vector2Type{a[0], a[1]};
  }
};
template <> struct Components<RectangleType> {
  static constexpr size_t N = 4;
  static std::array<float, 4> to(RectangleType r) {
    return {r.x, r.y, r.width, r.height};
  }
  static RectangleType from(const std::array<float, 4> &a) {
    return RectangleType{a[0], a[1], a[2], a[3]};
  }
};
template <> struct Components<ColorType> {
  static constexpr size_t N = 4;
  static std::array<float, 4> to(ColorType c) {
    return {float(c.r), float(c.g), float(c.b), float(c.a)};
  }
  static ColorType from(const std::array<float, 4> &a) {
    auto ch = [](float v) {
      return static_cast<unsigned char>(std::clamp(v + 0.5f, 0.f, 255.f));
    };
    return ColorType{ch(a[0]), ch(a[1]), ch(a[2]), ch(a[3])};
  }
};

inline bool &instant_flag() {
  static bool v = false;
  return v;
}
inline void set_instant(bool on) { instant_flag() = on; }
inline bool is_instant() { return instant_flag(); }

using Mode = std::variant<Spring, Timeline>;

template <typename T> struct Track {
  using C = Components<T>;
  static constexpr size_t N = C::N;
  using Vec = std::array<float, N>;

  struct Step {
    Vec target{};
    Mode mode{Spring{}};
    float delay = 0.f;
  };

  T value() const { return C::from(pos); }
  T value_or(T fallback) const { return has_started ? value() : fallback; }
  bool started() const { return has_started; }
  T target() const {
    if (!queue.empty())
      return C::from(queue.back().target);
    return C::from(is_active ? step.target : pos);
  }
  bool active() const { return is_active; }

  Track &from(T v) {
    has_started = true;
    pos = C::to(v);
    vel.fill(0.f);
    queue.clear();
    chain.clear();
    is_active = false;
    repeating = false;
    return *this;
  }

  Track &to(T v, Mode mode = Spring{}) {
    has_started = true;
    queue.clear();
    chain.clear();
    repeating = false;
    is_active = false;
    Step s{C::to(v), std::move(mode), 0.f};
    chain.push_back(s);
    begin(s);
    return *this;
  }

  Track &then(T v, Mode mode = Spring{}) {
    Step s{C::to(v), std::move(mode), 0.f};
    chain.push_back(s);
    if (!is_active)
      begin(s);
    else
      queue.push_back(s);
    return *this;
  }

  Track &delay(float seconds) {
    if (!chain.empty())
      chain.back().delay = seconds;
    if (!queue.empty())
      queue.back().delay = seconds;
    else if (is_active)
      delay_left = seconds;
    return *this;
  }

  Track &repeat(bool on = true) {
    repeating = on;
    return *this;
  }

  Track &essential(bool on = true) {
    is_essential = on;
    return *this;
  }

  Track &on_complete(std::function<void()> fn) {
    complete_cb = std::move(fn);
    return *this;
  }

  Track &on_change(std::function<int(float)> quantize,
                   std::function<void(int)> cb)
    requires(N == 1)
  {
    watchers.push_back(Watcher{std::move(quantize), std::nullopt,
                                std::move(cb)});
    return *this;
  }
  Track &on_step(float step, std::function<void(int)> cb)
    requires(N == 1)
  {
    return on_change(
        [step](float v) { return static_cast<int>(std::floor(v / step)); },
        std::move(cb));
  }

  void advance(float dt) {
    if (!is_active)
      return;
    if (instant_flag() && !is_essential) {
      while (!queue.empty()) {
        start_pos = step.target;
        step = queue.front();
        queue.pop_front();
      }
      land();
      finish();
      return;
    }
    dt = std::max(dt, 0.f);
    std::optional<float> cycle_budget;
    while (true) {
      if (delay_left > 0.f) {
        const float used = std::min(dt, delay_left);
        delay_left -= used;
        dt -= used;
        if (dt <= 0.f)
          return;
      }
      elapsed += dt;
      sample();
      notify();
      if (elapsed < settle_at)
        return;
      const float carry = elapsed - settle_at;
      land();
      if (!queue.empty()) {
        Step next = queue.front();
        queue.pop_front();
        begin(next);
      } else if (repeating && !chain.empty()) {
        if (cycle_budget && carry >= *cycle_budget)
          return;
        cycle_budget = carry;
        pos = chain_start;
        vel.fill(0.f);
        for (size_t i = 1; i < chain.size(); ++i)
          queue.push_back(chain[i]);
        begin(chain.front());
      } else {
        finish();
        return;
      }
      dt = carry;
    }
  }

private:
  struct Watcher {
    std::function<int(float)> quantize;
    std::optional<int> last_value;
    std::function<void(int)> callback;
  };

  void land() {
    if (std::holds_alternative<Timeline>(step.mode)) {
      elapsed = std::get<Timeline>(step.mode).length();
      sample();
    } else {
      pos = step.target;
    }
    vel.fill(0.f);
  }

  void begin(const Step &s) {
    if (!is_active)
      chain_start = pos;
    step = s;
    start_pos = pos;
    start_vel = vel;
    elapsed = 0.f;
    delay_left = s.delay;
    is_active = true;
    settle_at = std::visit(
        [&](const auto &m) {
          using M = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<M, Spring>) {
            float worst = 0.f;
            for (size_t i = 0; i < N; ++i)
              worst = std::max(
                  worst, spring_settle_time(
                             m, SpringState{start_pos[i], start_vel[i], step.target[i]}));
            return worst;
          } else {
            vel.fill(0.f);
            return m.repeat == Timeline::Repeat::Once
                       ? m.length()
                       : std::numeric_limits<float>::infinity();
          }
        },
        step.mode);
  }

  void sample() {
    std::visit(
        [&](const auto &m) {
          using M = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<M, Spring>) {
            for (size_t i = 0; i < N; ++i) {
              const auto smp =
                  spring_solve(m, SpringState{start_pos[i], start_vel[i], step.target[i]}, elapsed);
              pos[i] = smp.x;
              vel[i] = smp.v;
            }
          } else {
            const float p = m.at(elapsed);
            for (size_t i = 0; i < N; ++i)
              pos[i] = std::lerp(start_pos[i], step.target[i], p);
          }
        },
        step.mode);
  }

  void notify() {
    if constexpr (N == 1) {
      for (auto &w : watchers) {
        const int q = w.quantize(pos[0]);
        if (!w.last_value.has_value() || q != *w.last_value) {
          w.last_value = q;
          if (w.callback)
            w.callback(q);
        }
      }
    }
  }

  void finish() {
    is_active = false;
    notify();
    if (complete_cb)
      complete_cb();
  }

  Vec pos{}, vel{}, start_pos{}, start_vel{}, chain_start{};
  Step step{};
  std::deque<Step> queue;
  std::vector<Step> chain;
  float elapsed = 0.f;
  float settle_at = 0.f;
  float delay_left = 0.f;
  bool is_active = false;
  bool repeating = false;
  bool has_started = false;
  bool is_essential = false;
  std::function<void()> complete_cb;
  std::vector<Watcher> watchers;
};

} // namespace motion
} // namespace afterhours
