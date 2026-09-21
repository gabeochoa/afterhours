#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>

#include "../developer.h"

namespace afterhours {
namespace particles {

struct Particle {
  Vector2Type pos{};
  Vector2Type vel{};
  float size = 4.f;
  float rotation = 0.f;
  float spin = 0.f;
  float age = 0.f;
  float life = 1.f;
  ColorType color{255, 255, 255, 255};
  float progress() const { return life > 0.f ? std::clamp(age / life, 0.f, 1.f) : 1.f; }
};

template <size_t Capacity = 512> struct Emitter {
  std::array<Particle, Capacity> pool{};
  std::array<bool, Capacity> alive{};
  size_t next = 0;
  size_t live = 0;
  Vector2Type gravity{0.f, 0.f};
  float drag = 0.f;
  std::optional<float> floor_y;
  float restitution = 0.6f;

  Particle &spawn() {
    for (size_t tries = 0; tries < Capacity; ++tries) {
      const size_t i = next;
      next = (next + 1) % Capacity;
      if (!alive[i]) {
        alive[i] = true;
        ++live;
        pool[i] = Particle{};
        return pool[i];
      }
    }
    const size_t i = next;
    next = (next + 1) % Capacity;
    pool[i] = Particle{};
    return pool[i];
  }

  void clear() {
    alive.fill(false);
    live = 0;
    next = 0;
  }

  void update(float dt) {
    dt = std::clamp(dt, 0.f, 0.1f);
    for (size_t i = 0; i < Capacity; ++i) {
      if (!alive[i])
        continue;
      Particle &p = pool[i];
      p.age += dt;
      if (p.age >= p.life) {
        alive[i] = false;
        --live;
        continue;
      }
      p.vel.x += gravity.x * dt;
      p.vel.y += gravity.y * dt;
      p.vel.x -= p.vel.x * drag * dt;
      p.vel.y -= p.vel.y * drag * dt;
      p.pos.x += p.vel.x * dt;
      p.pos.y += p.vel.y * dt;
      p.rotation += p.spin * dt;
      if (floor_y && p.pos.y + p.size / 2.f > *floor_y && p.vel.y > 0.f) {
        p.pos.y = *floor_y - p.size / 2.f;
        p.vel.y = -p.vel.y * restitution;
        p.vel.x *= 0.85f;
        if (std::fabs(p.vel.y) < 20.f)
          p.vel.y = 0.f;
      }
    }
  }

  template <typename Fn> void each(Fn &&fn) const {
    for (size_t i = 0; i < Capacity; ++i)
      if (alive[i])
        fn(pool[i]);
  }

  size_t count() const { return live; }
};

inline float hash01(size_t n) {
  n ^= n >> 16;
  n *= 0x7feb352dU;
  n ^= n >> 15;
  n *= 0x846ca68bU;
  n ^= n >> 16;
  return static_cast<float>(n & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

} // namespace particles
} // namespace afterhours
