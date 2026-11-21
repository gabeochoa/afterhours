// character3d_test.cpp
// CharacterMotor over scripted height samplers: flat walking, slope refusal,
// step-up, falling and landing. Injected dt only, so every path is exact.

#include <afterhours/src/plugins/character3d.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

static bool near(float a, float b, float tol = 1e-4f) {
  return std::fabs(a - b) <= tol;
}

static CharacterMotor motor_at(float x, float y, float z) {
  CharacterMotor m;
  m.position = {x, y, z};
  m.grounded = true;
  return m;
}

int main() {
  const float dt = 0.1f;
  auto flat = [](float, float) { return 0.f; };

  // Flat walk: speed is exact, the motor stays grounded at ground height.
  {
    CharacterMotor m = motor_at(0.f, 0.f, 0.f);
    for (int i = 0; i < 10; i++) m.update(dt, {1.f, 0.f, 0.f}, 3.f, flat);
    check(near(m.position.x, 3.f), "flat walk covers speed * time");
    check(near(m.position.y, 0.f) && m.grounded, "flat walk stays grounded");
  }

  // Slope refusal: a 60-degree ramp refuses uphill past a 45-degree limit,
  // while a 20-degree ramp with the same limit climbs.
  {
    auto ramp60 = [](float x, float) { return x * 1.7320508f; }; // tan(60)
    CharacterMotor m = motor_at(0.f, 0.f, 0.f);
    m.max_slope_deg = 45.f;
    for (int i = 0; i < 10; i++) m.update(dt, {1.f, 0.f, 0.f}, 3.f, ramp60);
    check(near(m.position.x, 0.f), "slope beyond the limit refuses uphill");

    auto ramp20 = [](float x, float) { return x * 0.3639702f; }; // tan(20)
    CharacterMotor g = motor_at(0.f, 0.f, 0.f);
    g.max_slope_deg = 45.f;
    for (int i = 0; i < 10; i++) g.update(dt, {1.f, 0.f, 0.f}, 3.f, ramp20);
    check(g.position.x > 2.9f && g.grounded, "walkable slope climbs");
    check(near(g.position.y, g.position.x * 0.3639702f, 1e-3f),
          "climbing follows the slope height");
  }

  // Step-up: a ledge within step height is climbed; a taller one blocks.
  {
    auto ledge = [](float x, float) { return x >= 1.f ? 0.4f : 0.f; };
    CharacterMotor m = motor_at(0.f, 0.f, 0.f); // step_height 0.55 default
    for (int i = 0; i < 10; i++) m.update(dt, {1.f, 0.f, 0.f}, 3.f, ledge);
    check(m.position.x > 1.f && near(m.position.y, 0.4f) && m.grounded,
          "step-up climbs a ledge within step height");

    auto wall = [](float x, float) { return x >= 1.f ? 1.f : 0.f; };
    CharacterMotor b = motor_at(0.f, 0.f, 0.f);
    for (int i = 0; i < 10; i++) b.update(dt, {1.f, 0.f, 0.f}, 3.f, wall);
    check(b.position.x < 1.f && near(b.position.y, 0.f),
          "a ledge taller than step height blocks");
  }

  // Fall and land: gravity takes an airborne motor down to grounded.
  {
    CharacterMotor m;
    m.position = {0.f, 5.f, 0.f};
    bool landed = false;
    for (int i = 0; i < 200 && !landed; i++) {
      m.update(dt, {}, 0.f, flat);
      landed = m.grounded;
    }
    check(landed && near(m.position.y, 0.f), "falling lands grounded");
    check(near(m.velocity.y, 0.f), "landing zeroes fall speed");
  }

  // Walk off an edge: airborne over the drop, then grounded on the floor.
  {
    auto cliff = [](float x, float) { return x < 1.f ? 2.f : 0.f; };
    CharacterMotor m = motor_at(0.f, 2.f, 0.f);
    bool saw_airborne = false;
    for (int i = 0; i < 100; i++) {
      m.update(dt, {1.f, 0.f, 0.f}, 3.f, cliff);
      if (!m.grounded) saw_airborne = true;
    }
    check(saw_airborne, "walking off an edge goes airborne");
    check(m.grounded && near(m.position.y, 0.f), "and lands on the floor below");
  }

  // Determinism: the same injected steps replay to the identical position.
  {
    auto hills = [](float x, float z) { return 0.3f * std::sin(x) + 0.2f * std::cos(z); };
    auto run = [&] {
      CharacterMotor m = motor_at(0.f, hills(0.f, 0.f), 0.f);
      for (int i = 0; i < 50; i++) m.update(dt, {1.f, 0.f, 0.5f}, 2.f, hills);
      return m.position;
    };
    Vec3 a = run(), b = run();
    check(a.x == b.x && a.y == b.y && a.z == b.z, "same steps, same path");
  }

  std::printf("character3d: %d/%d checks passed\n", checks - failures, checks);
  return failures == 0 ? 0 : 1;
}
