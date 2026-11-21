#pragma once

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
    bool countdown = false;

    HasTimer() = default;
    explicit HasTimer(float reset_time, bool auto_reset = false)
        : reset_time(reset_time), current_time(0.0f), auto_reset(auto_reset),
          paused(false) {}

    // Countdown form: current_time holds REMAINING, from duration to 0.
    static HasTimer make_countdown(float duration, bool auto_reset = false) {
      HasTimer t;
      t.reset_time = duration;
      t.current_time = duration;
      t.auto_reset = auto_reset;
      t.countdown = true;
      return t;
    }

    [[nodiscard]] bool is_ready() const {
      return countdown ? current_time <= 0.0f : current_time >= reset_time;
    }

    [[nodiscard]] float get_progress() const {
      if (reset_time <= 0.0f)
        return 1.0f;
      return countdown ? 1.0f - current_time / reset_time
                       : current_time / reset_time;
    }

    [[nodiscard]] float get_remaining() const {
      return countdown ? std::max(0.0f, current_time)
                       : std::max(0.0f, reset_time - current_time);
    }

    void reset() { current_time = countdown ? reset_time : 0.0f; }

    void set_time(float time) { current_time = time; }

    void add_time(float delta) {
      if (!paused) {
        current_time += countdown ? -delta : delta;
      }
    }

    void pause() { paused = true; }

    void resume() { paused = false; }

    // Returns true if timer triggered (reached reset_time / hit 0)
    bool update(float delta) {
      if (paused)
        return false;

      if (countdown) {
        if (is_ready())
          return true;
        current_time -= delta;
        if (current_time <= 0.0f) {
          if (auto_reset) {
            reset();
          } else {
            current_time = 0.0f;
          }
          return true;
        }
        return false;
      }

      current_time += delta;
      if (is_ready()) {
        if (auto_reset) {
          reset();
        }
        return true;
      }
      return false;
    }

    // Pointer-free snapshot DTO: feeds snapshot::take_components<HasTimer,
    // HasTimerDTO> (see core/snapshot.h).
    struct HasTimerDTO {
      float reset_time = 0.0f;
      float current_time = 0.0f;
      bool auto_reset = false;
      bool paused = false;
      bool countdown = false;
    };

    [[nodiscard]] HasTimerDTO to_dto() const {
      return HasTimerDTO{.reset_time = reset_time,
                         .current_time = current_time,
                         .auto_reset = auto_reset,
                         .paused = paused,
                         .countdown = countdown};
    }

    static HasTimer from_dto(const HasTimerDTO &d) {
      HasTimer t;
      t.reset_time = d.reset_time;
      t.current_time = d.current_time;
      t.auto_reset = d.auto_reset;
      t.paused = d.paused;
      t.countdown = d.countdown;
      return t;
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

  // System to update all timers
  struct TimerUpdateSystem : System<HasTimer> {
    virtual void for_each_with(Entity &, HasTimer &timer,
                               float delta) override {
      timer.update(delta);
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
    sm.register_update_system(std::make_unique<TriggerUpdateSystem>());
  }
};

// Compile-time verification that timer satisfies the PluginCore concept
static_assert(developer::PluginCore<timer>,
              "timer must implement the core plugin interface");

} // namespace afterhours
