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
namespace detail {
inline float srgb_to_linear(float u) {
  return u <= 0.04045f ? u / 12.92f : std::pow((u + 0.055f) / 1.055f, 2.4f);
}
inline float linear_to_srgb(float u) {
  u = std::clamp(u, 0.f, 1.f);
  return u <= 0.0031308f ? u * 12.92f : 1.055f * std::pow(u, 1.f / 2.4f) - 0.055f;
}
inline std::array<float, 3> srgb_to_oklab(float r, float g, float b) {
  const float lr = srgb_to_linear(r), lg = srgb_to_linear(g), lb = srgb_to_linear(b);
  const float l = std::cbrt(0.4122214708f * lr + 0.5363325363f * lg + 0.0514459929f * lb);
  const float m = std::cbrt(0.2119034982f * lr + 0.6806995451f * lg + 0.1073969566f * lb);
  const float s = std::cbrt(0.0883024619f * lr + 0.2817188376f * lg + 0.6299787005f * lb);
  return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s,
          1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
          0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}
inline std::array<float, 3> oklab_to_srgb(float L, float a, float b) {
  const float l0 = L + 0.3963377774f * a + 0.2158037573f * b;
  const float m0 = L - 0.1055613458f * a - 0.0638541728f * b;
  const float s0 = L - 0.0894841775f * a - 1.2914855480f * b;
  const float l = l0 * l0 * l0, m = m0 * m0 * m0, s = s0 * s0 * s0;
  return {linear_to_srgb(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s),
          linear_to_srgb(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s),
          linear_to_srgb(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s)};
}
} // namespace detail

template <> struct Components<ColorType> {
  static constexpr size_t N = 4;
  static std::array<float, 4> to(ColorType c) {
    const auto lab = detail::srgb_to_oklab(c.r / 255.f, c.g / 255.f, c.b / 255.f);
    return {lab[0], lab[1], lab[2], float(c.a)};
  }
  static ColorType from(const std::array<float, 4> &a) {
    const auto rgb = detail::oklab_to_srgb(a[0], a[1], a[2]);
    auto ch = [](float v) {
      return static_cast<unsigned char>(std::clamp(v * 255.f + 0.5f, 0.f, 255.f));
    };
    const auto alpha = static_cast<unsigned char>(std::clamp(a[3] + 0.5f, 0.f, 255.f));
    return ColorType{ch(rgb[0]), ch(rgb[1]), ch(rgb[2]), alpha};
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
  float elapsed() const { return time; }
  T target() const {
    if (!queue.empty())
      return C::from(queue.back().target);
    return C::from(has_step ? step.target : pos);
  }
  bool active() const { return is_active; }

  Track &from(T v) {
    has_started = true;
    has_step = false;
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
      time += dt;
      sample();
      notify();
      if (time < settle_at)
        return;
      const float carry = time - settle_at;
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
      time = std::get<Timeline>(step.mode).length();
      sample();
    } else {
      pos = step.target;
    }
    vel.fill(0.f);
  }

  void begin(const Step &s) {
    has_step = true;
    if (!is_active)
      chain_start = pos;
    step = s;
    start_pos = pos;
    start_vel = vel;
    time = 0.f;
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
                  spring_solve(m, SpringState{start_pos[i], start_vel[i], step.target[i]}, time);
              pos[i] = smp.x;
              vel[i] = smp.v;
            }
          } else {
            const float p = m.at(time);
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
  float time = 0.f;
  float settle_at = 0.f;
  float delay_left = 0.f;
  bool is_active = false;
  bool repeating = false;
  bool has_started = false;
  bool has_step = false;
  bool is_essential = false;
  std::function<void()> complete_cb;
  std::vector<Watcher> watchers;
};

} // namespace motion
} // namespace afterhours
