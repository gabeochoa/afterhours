#pragma once

#include <cstdint>
#include <functional>

#include "entity_helper.h"
#include "system.h"

namespace afterhours {

struct SimulationDriver {
    enum class State { Idle, Running, Paused, Finished };

    SystemManager systems;
    float speed = 1.f;

    State state() const { return sim_state; }
    bool running() const { return sim_state == State::Running; }
    bool finished() const { return sim_state == State::Finished; }
    uint64_t tick_count() const { return tick_count_; }
    float fixed_step() const { return systems.fixed_tick_rate; }
    float elapsed() const { return static_cast<float>(tick_count_) * systems.fixed_tick_rate; }

    void set_fixed_step(float dt) { systems.fixed_tick_rate = dt; }

    void begin() {
        if (sim_state == State::Idle) sim_state = State::Running;
    }
    void pause() {
        if (sim_state == State::Running) sim_state = State::Paused;
    }
    void resume() {
        if (sim_state == State::Paused) sim_state = State::Running;
    }
    void finish() {
        if (sim_state == State::Running || sim_state == State::Paused) sim_state = State::Finished;
    }
    void reset() {
        sim_state = State::Idle;
        tick_count_ = 0;
        carry = 0.f;
        systems.accumulator = 0.f;
    }

    int step(int n = 1) {
        if (!running() || n <= 0) return 0;
        VALIDATE(systems.fixed_tick_rate > 0.f, "fixed_tick_rate must be positive");
        auto &entities = EntityHelper::get_entities_for_mod();
        for (int i = 0; i < n; ++i) {
            const float dt = systems.fixed_tick_rate;
            systems.fixed_tick(entities, dt);
            systems.tick(entities, dt);
            EntityHelper::cleanup();
            ++tick_count_;
        }
        return n;
    }

    int advance(float dt) {
        if (!running() || dt <= 0.f || speed <= 0.f) return 0;
        VALIDATE(systems.fixed_tick_rate > 0.f, "fixed_tick_rate must be positive");
        carry += dt * speed;
        const int n = static_cast<int>(carry / systems.fixed_tick_rate);
        if (n <= 0) return 0;
        carry -= static_cast<float>(n) * systems.fixed_tick_rate;
        return step(n);
    }

    bool run_until(const std::function<bool()> &done, int max_ticks) {
        if (done && done()) {
            if (!running()) return false;
            finish();
            return true;
        }
        for (int i = 0; i < max_ticks; ++i) {
            if (step(1) == 0) return false;
            if (done && done()) {
                finish();
                return true;
            }
        }
        return false;
    }

   private:
    State sim_state = State::Idle;
    uint64_t tick_count_ = 0;
    float carry = 0.f;
};

}
