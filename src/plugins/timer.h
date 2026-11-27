#pragma once

#include <optional>

#include "../core/base_component.h"
#include "../ecs.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

namespace afterhours {

struct timer : developer::Plugin {
  // Component for entities with timers/cooldowns
  struct HasTimer : BaseComponent {
    float reset_time = 0.0f;
    float current_time = 0.0f;
    bool auto_reset = false;
    bool paused = false;

    HasTimer() = default;

    explicit HasTimer(float reset_time, bool auto_reset = false)
        : reset_time(reset_time), current_time(0.0f), auto_reset(auto_reset),
          paused(false) {}

    [[nodiscard]] bool is_ready() const { return current_time >= reset_time; }

    [[nodiscard]] float get_progress() const {
      if (reset_time <= 0.0f)
        return 1.0f;
      return current_time / reset_time;
    }

    [[nodiscard]] float get_remaining() const {
      return std::max(0.0f, reset_time - current_time);
    }

    void reset() { current_time = 0.0f; }

    void set_time(float time) { current_time = time; }

    void add_time(float delta) {
      if (!paused) {
        current_time += delta;
      }
    }

    void pause() { paused = true; }

    void resume() { paused = false; }

    // Returns true if timer triggered (reached reset_time)
    bool update(float delta) {
      if (paused)
        return false;

      current_time += delta;
      if (is_ready()) {
        if (auto_reset) {
          reset();
        }
        return true;
      }
      return false;
    }
  };

  struct HasCountdownSerializable {
    float duration = 0.0f;
    float remaining = 0.0f;
    bool auto_reset = false;
    bool paused = false;
  };

  struct HasCountdown : BaseComponent {
    float duration = 0.0f;
    float remaining = 0.0f;
    bool auto_reset = false;
    bool paused = false;

    HasCountdown() = default;

    explicit HasCountdown(float duration, bool auto_reset = false)
        : duration(duration), remaining(duration), auto_reset(auto_reset),
          paused(false) {}

    [[nodiscard]] bool is_ready() const { return remaining <= 0.0f; }

    [[nodiscard]] float get_progress() const {
      if (duration <= 0.0f)
        return 1.0f;
      return 1.0f - get_remaining() / duration;
    }

    [[nodiscard]] float get_remaining() const {
      return std::max(0.0f, remaining);
    }

    void reset() { remaining = duration; }

    void set_remaining(float time) { remaining = time; }

    void add_time(float delta) {
      if (!paused) {
        remaining -= delta;
      }
    }

    void pause() { paused = true; }

    void resume() { paused = false; }

    bool update(float delta) {
      if (paused)
        return false;

      if (is_ready())
        return true;

      remaining -= delta;
      if (remaining <= 0.0f) {
        if (auto_reset) {
          reset();
        } else {
          remaining = 0.0f;
        }
        return true;
      }
      return false;
    }

    [[nodiscard]] HasCountdownSerializable to_serializable() const {
      return HasCountdownSerializable{.duration = duration,
                             .remaining = remaining,
                             .auto_reset = auto_reset,
                             .paused = paused};
    }

    static HasCountdown from_serializable(const HasCountdownSerializable &serializable) {
      HasCountdown countdown;
      countdown.duration = serializable.duration;
      countdown.remaining = serializable.remaining;
      countdown.auto_reset = serializable.auto_reset;
      countdown.paused = serializable.paused;
      return countdown;
    }
  };

  // Component for one-shot triggers
  struct TriggerOnDt : BaseComponent {
    float reset;
    float current;

    explicit TriggerOnDt(float time_to_pass)
        : reset(time_to_pass), current(0.0f) {}

    [[nodiscard]] bool test(float dt) {
      current += dt;
      if (current >= reset) {
        current = 0.0f;
        return true;
      }
      return false;
    }

    void reset_timer() { current = 0.0f; }
  };

  // Earliest pending deadline across registered timers, in seconds (the same
  // units update() advances). Paused and already-ready timers never become
  // due on their own, so they do not count; none pending -> nullopt.
  [[nodiscard]] static inline std::optional<float> next_deadline(
      EntityCollection &ec) {
    std::optional<float> best;
    EntityQuery(ec).whereHasComponent<HasTimer>().for_each([&](Entity &e) {
      const auto &t = e.get<HasTimer>();
      const float r = t.paused ? -1.f : t.get_remaining();
      if (r > 0.f && (!best || r < *best)) best = r;
    });
    EntityQuery(ec).whereHasComponent<TriggerOnDt>().for_each([&](Entity &e) {
      const auto &t = e.get<TriggerOnDt>();
      const float r = t.reset - t.current;
      if (r > 0.f && (!best || r < *best)) best = r;
    });
    return best;
  }

  // System to update all timers
  struct TimerUpdateSystem : System<HasTimer> {
    virtual void for_each_with(Entity &, HasTimer &timer,
                               float delta) override {
      timer.update(delta);
    }
  };

  struct CountdownUpdateSystem : System<HasCountdown> {
    virtual void for_each_with(Entity &, HasCountdown &countdown,
                               float delta) override {
      countdown.update(delta);
    }
  };

  // System to update triggers
  struct TriggerUpdateSystem : System<TriggerOnDt> {
    virtual void for_each_with(Entity &, TriggerOnDt &trigger,
                               float delta) override {
      trigger.test(delta);
    }
  };

  static void add_singleton_components(Entity &) {}

  static void enforce_singletons(SystemManager &) {}

  static void register_update_systems(SystemManager &sm) {
    sm.register_update_system(std::make_unique<TimerUpdateSystem>());
    sm.register_update_system(std::make_unique<CountdownUpdateSystem>());
    sm.register_update_system(std::make_unique<TriggerUpdateSystem>());
  }
};

// Compile-time verification that timer satisfies the PluginCore concept
static_assert(developer::PluginCore<timer>,
              "timer must implement the core plugin interface");

} // namespace afterhours
