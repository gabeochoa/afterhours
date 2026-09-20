#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include "../core/system.h"
#include "../developer.h"

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

struct Spring {
  float response = 0.3f;
  float bounce = 0.f;
  float rest_delta = 0.f;
  float rest_speed = 0.f;
};

struct SpringState {
  float x0 = 0.f;
  float v0 = 0.f;
  float target = 0.f;
};

struct SpringSample {
  float x = 0.f;
  float v = 0.f;
};

static float spring_omega(const Spring &s) {
  return 2.f * 3.14159265358979f / std::max(s.response, 1e-4f);
}
static float spring_zeta(const Spring &s) {
  return 1.f - std::clamp(s.bounce, 0.f, 0.999f);
}

static SpringSample spring_solve(const Spring &s, const SpringState &st,
                                 float t) {
  t = std::max(t, 0.f);
  const float w = spring_omega(s);
  const float z = spring_zeta(s);
  const float d0 = st.x0 - st.target;
  const float v0 = st.v0;
  const float decay = std::exp(-z * w * t);
  if (z > 0.9999f) {
    const float c = v0 + w * d0;
    const float d = decay * (d0 + c * t);
    const float v = decay * (v0 - w * c * t);
    return {st.target + d, v};
  }
  const float wd = w * std::sqrt(1.f - z * z);
  const float a = d0;
  const float b = (v0 + z * w * d0) / wd;
  const float cs = std::cos(wd * t), sn = std::sin(wd * t);
  const float d = decay * (a * cs + b * sn);
  const float v = decay * ((b * wd - z * w * a) * cs -
                           (a * wd + z * w * b) * sn);
  return {st.target + d, v};
}

static float spring_settle_time(const Spring &s, const SpringState &st) {
  const float w = spring_omega(s);
  const float z = spring_zeta(s);
  const float d0 = st.x0 - st.target;
  const float v0 = st.v0;
  float amp_d, amp_v, grow_d = 0.f, grow_v = 0.f;
  if (z > 0.9999f) {
    const float c = v0 + w * d0;
    amp_d = std::fabs(d0);
    grow_d = std::fabs(c);
    amp_v = std::fabs(v0);
    grow_v = w * std::fabs(c);
  } else {
    const float wd = w * std::sqrt(1.f - z * z);
    const float b = (v0 + z * w * d0) / wd;
    amp_d = std::sqrt(d0 * d0 + b * b);
    const float vb = (w * w * d0 + z * w * v0) / wd;
    amp_v = std::sqrt(v0 * v0 + vb * vb);
  }
  const float scale = std::max(amp_d, amp_v / w);
  if (scale <= 0.f)
    return 0.f;
  const float eps_d = s.rest_delta > 0.f ? s.rest_delta : scale * 1e-3f;
  const float eps_v = s.rest_speed > 0.f ? s.rest_speed : eps_d * w;
  auto under = [&](float t) {
    const float decay = std::exp(-z * w * t);
    return decay * (amp_d + grow_d * t) <= eps_d &&
           decay * (amp_v + grow_v * t) <= eps_v;
  };
  float hi = s.response;
  for (int i = 0; i < 32 && !under(hi); ++i)
    hi *= 2.f;
  float lo = 0.f;
  for (int i = 0; i < 32; ++i) {
    const float mid = 0.5f * (lo + hi);
    (under(mid) ? hi : lo) = mid;
  }
  return hi;
}

struct Timeline {
  struct Key {
    float t = 0.f;
    float v = 0.f;
  };
  enum struct Repeat { Once, Loop, PingPong };
  std::vector<Key> keys;
  Repeat repeat = Repeat::Once;
  float (*curve)(float) = nullptr;

  float length() const { return keys.empty() ? 0.f : keys.back().t; }
  bool finished(float t) const {
    return repeat == Repeat::Once && t >= length();
  }
  float at(float t) const {
    if (keys.empty())
      return 0.f;
    const float len = length();
    if (len <= 0.f)
      return keys.back().v;
    t = std::max(t, 0.f);
    switch (repeat) {
    case Repeat::Once:
      t = std::min(t, len);
      break;
    case Repeat::Loop:
      t = std::fmod(t, len);
      break;
    case Repeat::PingPong: {
      const float cycle = std::fmod(t, 2.f * len);
      t = cycle <= len ? cycle : 2.f * len - cycle;
      break;
    }
    }
    if (t <= keys.front().t)
      return keys.front().v;
    const auto next = std::upper_bound(
        keys.begin(), keys.end(), t,
        [](float value, const Key &k) { return value < k.t; });
    if (next == keys.end())
      return keys.back().v;
    const Key &a = *(next - 1);
    const Key &b = *next;
    const float span = b.t - a.t;
    float u = span > 0.f ? (t - a.t) / span : 1.f;
    if (curve)
      u = curve(u);
    return std::lerp(a.v, b.v, u);
  }
};

// Instant mode: every animation lands on its final value on the first update.
//
// Deliberately not an enabled/disabled flag. The animation code still runs and
// on_complete still fires, so nothing downstream has to branch on it -- which
// is what makes it safe to leave on. Three callers want the same knob: e2e
// wants screenshots of the settled state, accessibility wants reduce-motion,
// and dev iteration wants to skip the wait.
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

  T value() const { return C::from(x_); }
  T target() const {
    if (!queue_.empty())
      return C::from(queue_.back().target);
    return C::from(active_ ? step_.target : x_);
  }
  bool active() const { return active_; }

  Track &from(T v) {
    x_ = C::to(v);
    v_.fill(0.f);
    queue_.clear();
    chain_.clear();
    active_ = false;
    repeat_ = false;
    return *this;
  }

  Track &to(T v, Mode mode = Spring{}) {
    queue_.clear();
    chain_.clear();
    repeat_ = false;
    active_ = false;
    Step s{C::to(v), std::move(mode), 0.f};
    chain_.push_back(s);
    begin(s);
    return *this;
  }

  Track &then(T v, Mode mode = Spring{}) {
    Step s{C::to(v), std::move(mode), 0.f};
    chain_.push_back(s);
    if (!active_)
      begin(s);
    else
      queue_.push_back(s);
    return *this;
  }

  Track &delay(float seconds) {
    if (!chain_.empty())
      chain_.back().delay = seconds;
    if (!queue_.empty())
      queue_.back().delay = seconds;
    else if (active_)
      remaining_delay_ = seconds;
    return *this;
  }

  Track &repeat(bool on = true) {
    repeat_ = on;
    return *this;
  }

  Track &on_complete(std::function<void()> fn) {
    on_complete_ = std::move(fn);
    return *this;
  }

  Track &on_change(std::function<int(float)> quantize,
                   std::function<void(int)> cb)
    requires(N == 1)
  {
    watchers_.push_back(Watcher{std::move(quantize), std::nullopt,
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
    if (!active_)
      return;
    if (instant_flag()) {
      while (!queue_.empty()) {
        x0_ = step_.target;
        step_ = queue_.front();
        queue_.pop_front();
      }
      land();
      finish();
      return;
    }
    dt = std::max(dt, 0.f);
    std::optional<float> cycle_budget;
    while (true) {
      if (remaining_delay_ > 0.f) {
        const float used = std::min(dt, remaining_delay_);
        remaining_delay_ -= used;
        dt -= used;
        if (dt <= 0.f)
          return;
      }
      t_ += dt;
      sample();
      notify();
      if (t_ < settle_)
        return;
      const float carry = t_ - settle_;
      land();
      if (!queue_.empty()) {
        Step next = queue_.front();
        queue_.pop_front();
        begin(next);
      } else if (repeat_ && !chain_.empty()) {
        if (cycle_budget && carry >= *cycle_budget)
          return;
        cycle_budget = carry;
        x_ = start_x_;
        v_.fill(0.f);
        for (size_t i = 1; i < chain_.size(); ++i)
          queue_.push_back(chain_[i]);
        begin(chain_.front());
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
    if (std::holds_alternative<Timeline>(step_.mode)) {
      t_ = std::get<Timeline>(step_.mode).length();
      sample();
    } else {
      x_ = step_.target;
    }
    v_.fill(0.f);
  }

  void begin(const Step &s) {
    if (!active_)
      start_x_ = x_;
    step_ = s;
    x0_ = x_;
    v0_ = v_;
    t_ = 0.f;
    remaining_delay_ = s.delay;
    active_ = true;
    settle_ = std::visit(
        [&](const auto &m) {
          using M = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<M, Spring>) {
            float worst = 0.f;
            for (size_t i = 0; i < N; ++i)
              worst = std::max(
                  worst, spring_settle_time(
                             m, SpringState{x0_[i], v0_[i], step_.target[i]}));
            return worst;
          } else {
            v_.fill(0.f);
            return m.repeat == Timeline::Repeat::Once
                       ? m.length()
                       : std::numeric_limits<float>::infinity();
          }
        },
        step_.mode);
  }

  void sample() {
    std::visit(
        [&](const auto &m) {
          using M = std::decay_t<decltype(m)>;
          if constexpr (std::is_same_v<M, Spring>) {
            for (size_t i = 0; i < N; ++i) {
              const auto smp =
                  spring_solve(m, SpringState{x0_[i], v0_[i], step_.target[i]}, t_);
              x_[i] = smp.x;
              v_[i] = smp.v;
            }
          } else {
            const float p = m.at(t_);
            for (size_t i = 0; i < N; ++i)
              x_[i] = std::lerp(x0_[i], step_.target[i], p);
          }
        },
        step_.mode);
  }

  void notify() {
    if constexpr (N == 1) {
      for (auto &w : watchers_) {
        const int q = w.quantize(x_[0]);
        if (!w.last_value.has_value() || q != *w.last_value) {
          w.last_value = q;
          if (w.callback)
            w.callback(q);
        }
      }
    }
  }

  void finish() {
    active_ = false;
    notify();
    if (on_complete_)
      on_complete_();
  }

  Vec x_{}, v_{}, x0_{}, v0_{}, start_x_{};
  Step step_{};
  std::deque<Step> queue_;
  std::vector<Step> chain_;
  float t_ = 0.f;
  float settle_ = 0.f;
  float remaining_delay_ = 0.f;
  bool active_ = false;
  bool repeat_ = false;
  std::function<void()> on_complete_;
  std::vector<Watcher> watchers_;
};

} // namespace motion

struct animation : developer::Plugin {
  enum struct EasingType { Linear, EaseOutQuad, Hold };

  struct AnimSegment {
    float to_value = 0.f;
    float duration = 0.f;
    EasingType easing = EasingType::Linear;
  };

  struct AnimTrack {
    float current = 0.f;
    float from = 0.f;
    float to = 0.f;
    float duration = 0.f;
    float elapsed = 0.f;
    bool active = false;
    EasingType current_easing = EasingType::Linear;
    std::deque<AnimSegment> queue;
    std::function<void()> on_complete;
    struct Watcher {
      std::function<int(float)> quantize;
      std::optional<int> last_value;
      std::function<void(int)> callback;
    };
    std::vector<Watcher> watchers;
  };

  template <typename Enum> struct EnumHash {
    size_t operator()(Enum e) const noexcept {
      using U = std::underlying_type_t<Enum>;
      return static_cast<size_t>(static_cast<U>(e));
    }
  };

  struct CompositeKey {
    size_t base = 0;
    size_t index = 0;
    bool operator==(const CompositeKey &other) const noexcept {
      return base == other.base && index == other.index;
    }
  };

  struct CompositeKeyHash {
    size_t operator()(const CompositeKey &k) const noexcept {
      size_t h = k.base * 1469598103934665603ull;
      h ^= k.index + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
      return h;
    }
  };

  template <typename E>
  static inline CompositeKey make_key(E base, size_t index) {
    return CompositeKey{static_cast<size_t>(base), index};
  }

  // Hasher trait: default to EnumHash, specialize for CompositeKey
  template <typename Key> struct KeyHasher {
    using type = std::conditional_t<std::is_same_v<Key, CompositeKey>,
                                    CompositeKeyHash, EnumHash<Key>>;
  };

  static float apply_ease(EasingType easing, float t) {
    t = std::clamp(t, 0.f, 1.f);
    switch (easing) {
    case EasingType::Linear:
      return t;
    case EasingType::EaseOutQuad:
      return 1.f - (1.f - t) * (1.f - t);
    case EasingType::Hold:
      return 0.f;
    }
    return t;
  }

  using Spring = motion::Spring;
  using SpringState = motion::SpringState;
  using SpringSample = motion::SpringSample;
  using Timeline = motion::Timeline;
  using Mode = motion::Mode;
  template <typename T> using Track = motion::Track<T>;
  static float spring_omega(const Spring &s) { return motion::spring_omega(s); }
  static float spring_zeta(const Spring &s) { return motion::spring_zeta(s); }
  static SpringSample spring_solve(const Spring &s, const SpringState &st,
                                   float t) {
    return motion::spring_solve(s, st, t);
  }
  static float spring_settle_time(const Spring &s, const SpringState &st) {
    return motion::spring_settle_time(s, st);
  }
  static inline bool &instant_flag() { return motion::instant_flag(); }
  static void set_instant(bool on) { motion::set_instant(on); }
  static bool is_instant() { return motion::is_instant(); }

  template <typename Key> struct AnimationManager {
    using Hasher = typename KeyHasher<Key>::type;

    void update(float dt) {
      for (auto &kv : tracks) {
        AnimTrack &tr = kv.second;
        if (!tr.active)
          continue;

        if (tr.duration <= 0.f || instant_flag()) {
          // Skip to the END of the whole animation, not just this segment --
          // a queued sequence's last value is the one it was going to settle
          // on, and stopping at segment one would be a different picture.
          while (!tr.queue.empty()) {
            tr.to = tr.queue.front().to_value;
            tr.queue.pop_front();
          }
          tr.current = tr.to;
          tr.active = false;
          if (tr.on_complete)
            tr.on_complete();
        } else {
          tr.elapsed += dt;
          float u = apply_ease(tr.current_easing, tr.elapsed / tr.duration);
          float new_current = std::lerp(tr.from, tr.to, u);
          // notify watchers if quantized value changed during this
          // update
          if (!tr.watchers.empty()) {
            for (auto &w : tr.watchers) {
              int q = w.quantize(new_current);
              if (!w.last_value.has_value() || q != *w.last_value) {
                w.last_value = q;
                if (w.callback)
                  w.callback(q);
              }
            }
          }
          tr.current = new_current;
          if (tr.elapsed >= tr.duration) {
            tr.current = tr.to;
            if (!tr.queue.empty()) {
              auto seg = tr.queue.front();
              tr.queue.pop_front();
              tr.from = tr.current;
              tr.to = seg.to_value;
              tr.duration = seg.duration;
              tr.current_easing = seg.easing;
              tr.elapsed = 0.f;
              tr.active = true;
            } else {
              tr.active = false;
              if (tr.on_complete)
                tr.on_complete();
            }
          }
        }
      }
    }

    AnimTrack &ensure_track(Key key) { return tracks[key]; }
    /// Drop every track. A screen change leaves the previous screen's
    /// animations in here otherwise, still ticking against keys nothing reads.
    void clear_all() { tracks.clear(); }
    bool is_active(Key key) const {
      auto it = tracks.find(key);
      return it != tracks.end() && it->second.active;
    }
    std::optional<float> get_value(Key key) const {
      auto it = tracks.find(key);
      if (it == tracks.end() || !it->second.active)
        return std::nullopt;
      return it->second.current;
    }

  private:
    std::unordered_map<Key, AnimTrack, Hasher> tracks;
  };

  template <typename Key> struct AnimHandle {
    Key key;
    AnimationManager<Key> &mgr;

    AnimHandle &from(float value) {
      AnimTrack &tr = mgr.ensure_track(key);
      tr.current = value;
      tr.from = value;
      tr.to = value;
      tr.duration = 0.f;
      tr.elapsed = 0.f;
      tr.active = false;
      tr.queue.clear();
      tr.on_complete = nullptr;
      tr.watchers.clear();
      return *this;
    }
    AnimHandle &to(float value, float duration, EasingType easing) {
      AnimTrack &tr = mgr.ensure_track(key);
      if (!tr.active && tr.queue.empty()) {
        tr.from = tr.current;
        tr.to = value;
        tr.duration = duration;
        tr.current_easing = easing;
        tr.elapsed = 0.f;
        tr.active = true;
      } else {
        tr.queue.push_back(AnimSegment{
            .to_value = value, .duration = duration, .easing = easing});
      }
      return *this;
    }
    AnimHandle &sequence(const std::vector<AnimSegment> &segments) {
      AnimTrack &tr = mgr.ensure_track(key);
      if (segments.empty())
        return *this;
      if (!tr.active && tr.queue.empty()) {
        tr.from = tr.current;
        tr.to = segments[0].to_value;
        tr.duration = segments[0].duration;
        tr.current_easing = segments[0].easing;
        tr.elapsed = 0.f;
        tr.active = true;
        for (size_t i = 1; i < segments.size(); ++i)
          tr.queue.push_back(segments[i]);
      } else {
        for (auto &s : segments)
          tr.queue.push_back(s);
      }
      return *this;
    }
    AnimHandle &hold(float duration) {
      AnimTrack &tr = mgr.ensure_track(key);
      tr.queue.push_back(AnimSegment{.to_value = tr.current,
                                     .duration = duration,
                                     .easing = EasingType::Hold});
      return *this;
    }
    AnimHandle &on_complete(std::function<void()> callback) {
      AnimTrack &tr = mgr.ensure_track(key);
      tr.on_complete = std::move(callback);
      return *this;
    }
    AnimHandle &on_change(std::function<int(float)> quantize,
                          std::function<void(int)> cb) {
      AnimTrack &tr = mgr.ensure_track(key);
      tr.watchers.push_back(typename AnimTrack::Watcher{
          std::move(quantize), std::nullopt, std::move(cb)});
      return *this;
    }
    AnimHandle &on_step(float step, std::function<void(int)> cb) {
      return on_change(
          [step](float v) { return static_cast<int>(std::floor(v / step)); },
          std::move(cb));
    }
    AnimHandle &loop_sequence(const std::vector<AnimSegment> &segments) {
      sequence(segments);
      Key k = key;
      auto segs = segments;
      on_complete([k, segs]() mutable { anim<Key>(k).sequence(segs); });
      return *this;
    }
    float value() const {
      auto v = mgr.get_value(key);
      return v.value_or(0.f);
    }
    bool is_active() const { return mgr.is_active(key); }
  };

  template <typename Key> static inline AnimationManager<Key> &manager() {
    static AnimationManager<Key> m;
    return m;
  }
  template <typename Key> static inline AnimHandle<Key> anim(Key key) {
    return AnimHandle<Key>{key, manager<Key>()};
  }

  template <typename E>
  static inline AnimHandle<CompositeKey> anim(E base, size_t index) {
    return AnimHandle<CompositeKey>{make_key(base, index),
                                    manager<CompositeKey>()};
  }

  template <typename E>
  static inline std::optional<float> get_value(E base, size_t index) {
    return manager<CompositeKey>().get_value(make_key(base, index));
  }

  template <typename Key>
  static inline float clamp_value(Key key, float mn, float mx) {
    auto v = manager<Key>().get_value(key);
    float x = v.value_or(mx);
    return std::clamp(x, mn, mx);
  }

  template <typename E>
  static inline float clamp_value(E base, size_t index, float mn, float mx) {
    auto v = get_value(base, index);
    float x = v.value_or(mx);
    return std::clamp(x, mn, mx);
  }

  template <typename Key, typename Fn>
  static inline void one_shot(Key key, Fn fn) {
    using Hasher = typename KeyHasher<Key>::type;
    static std::unordered_set<Key, Hasher> started;
    if (started.contains(key))
      return;
    started.insert(key);
    fn(anim<Key>(key));
  }

  template <typename E, typename Fn>
  static inline void one_shot(E base, size_t index, Fn fn) {
    CompositeKey k = make_key(base, index);
    static std::unordered_set<CompositeKey, CompositeKeyHash> started;
    if (started.contains(k))
      return;
    started.insert(k);
    fn(anim(base, index));
  }

  // No singleton components needed for animation plugin
  static void add_singleton_components(Entity &) {}

  // No singleton enforcement needed for animation plugin
  static void enforce_singletons(SystemManager &) {}

  // Non-templated version for PluginCore concept compatibility
  static void register_update_systems(SystemManager &) {
    // Note: For actual animation updates, use the templated version:
    // animation::register_update_systems<YourKeyEnum>(sm);
  }

  // Templated version for registering animation updates with a specific key
  // type
  template <typename Key>
  static inline void register_update_systems(SystemManager &sm) {
    sm.register_update_system([](float dt) { manager<Key>().update(dt); });
  }
};

// Compile-time verification that animation satisfies the PluginCore concept
static_assert(developer::PluginCore<animation>,
              "animation must implement the core plugin interface");

} // namespace afterhours
