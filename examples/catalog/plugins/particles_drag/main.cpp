#include <cassert>
#include <cmath>
#include <cstdio>

#include "../../../../src/plugins/particles.h"

using namespace afterhours::particles;

static float speed_after(float dt, int steps) {
  Emitter<4> em;
  em.drag = 10.f;
  auto &p = em.spawn();
  p.vel = {100.f, 0.f};
  p.life = 10.f;
  for (int i = 0; i < steps; ++i)
    em.update(dt);
  return p.vel.x;
}

int main() {
  const float low = speed_after(0.05f, 10);
  const float normal = speed_after(1.f / 60.f, 30);
  const float high = speed_after(0.005f, 100);
  const float expected = 100.f * std::exp(-5.f);
  std::printf("speed after 0.5s with drag=10: 20fps %.4f, 60fps %.4f, 200fps %.4f, exp(-5)*100 %.4f\n",
              static_cast<double>(low), static_cast<double>(normal), static_cast<double>(high),
              static_cast<double>(expected));
  assert(std::fabs(low - expected) < 0.01f);
  assert(std::fabs(normal - expected) < 0.01f);
  assert(std::fabs(high - expected) < 0.01f);

  Emitter<4> em;
  em.drag = 25.f;
  auto &p = em.spawn();
  p.vel = {100.f, -50.f};
  p.life = 10.f;
  for (int i = 0; i < 2; ++i) {
    em.update(0.1f);
    assert(p.vel.x > 0.f);
    assert(p.vel.y < 0.f);
  }
  std::printf("low-fps steps kept velocity sign: vel = (%.4f, %.4f)\n", static_cast<double>(p.vel.x),
              static_cast<double>(p.vel.y));
  std::printf("particles drag example passed\n");
  return 0;
}
