#pragma once

#include "../core/base_component.h"
#include "../core/entity_helper.h"
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
            : reset_time(reset_time),
              current_time(0.0f),
              auto_reset(auto_reset),
              paused(false) {}

        [[nodiscard]] bool is_ready() const {
            return current_time >= reset_time;
        }

        [[nodiscard]] float get_progress() const {
            if (reset_time <= 0.0f) return 1.0f;
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
            if (paused) return false;

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

}  // namespace afterhours
