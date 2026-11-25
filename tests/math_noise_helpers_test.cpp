// math_noise_helpers_test.cpp
// Stateless hash01/value_noise and the math_helpers plugin: determinism,
// ranges, and the interpolation properties consumers rely on.

#include <cmath>
#include <cstdio>
#include <string>

#include <afterhours/src/plugins/math_helpers.h>
#include <afterhours/src/random_engine.h>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static bool near(float a, float b, float eps = 1e-5f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  bool deterministic = true;
  bool in_range = true;
  for (int x = -8; x < 8; x++) {
    for (int y = -8; y < 8; y++) {
      const float first = random::hash01(x, y, 7);
      deterministic = deterministic && first == random::hash01(x, y, 7);
      in_range = in_range && first >= 0.0f && first < 1.0f;
    }
  }
  check(deterministic, "hash01 is deterministic");
  check(in_range, "hash01 stays in [0, 1)");
  check(random::hash01(3, 4, 1) != random::hash01(3, 4, 2),
        "hash01 varies with the salt");
  check(random::hash01(3, 4) != random::hash01(4, 3),
        "hash01 varies with position");

  bool noise_bounded = true;
  bool noise_continuous = true;
  for (float x = -4.0f; x < 4.0f; x += 0.37f) {
    for (float y = -4.0f; y < 4.0f; y += 0.41f) {
      const float value = random::value_noise(x, y, 3);
      noise_bounded = noise_bounded && value >= 0.0f && value <= 1.0f;
      noise_continuous = noise_continuous &&
                         std::fabs(random::value_noise(x + 0.01f, y, 3) - value) <
                             0.05f;
    }
  }
  check(noise_bounded, "value_noise stays in [0, 1]");
  check(noise_continuous, "value_noise is continuous across lattice cells");
  check(near(random::value_noise(2.0f, 5.0f, 9), random::hash01(2, 5, 9)),
        "value_noise equals hash01 on lattice points");

  check(near(math::smoothstep(0.0f, 1.0f, -1.0f), 0.0f),
        "smoothstep clamps below the first edge");
  check(near(math::smoothstep(0.0f, 1.0f, 2.0f), 1.0f),
        "smoothstep clamps above the second edge");
  check(near(math::smoothstep(0.0f, 1.0f, 0.5f), 0.5f),
        "smoothstep passes through the midpoint");
  check(near(math::smoothstep(1.0f, 1.0f, 0.5f), 0.0f) &&
            near(math::smoothstep(1.0f, 1.0f, 1.5f), 1.0f),
        "smoothstep with a degenerate edge acts as a step");

  const float one_step = math::damp(0.0f, 10.0f, 5.0f, 0.1f);
  float many_steps = 0.0f;
  for (int i = 0; i < 10; i++)
    many_steps = math::damp(many_steps, 10.0f, 5.0f, 0.01f);
  check(near(one_step, many_steps, 1e-4f),
        "damp is frame-rate independent");

  check(near(math::wrap_angle(PI), -PI), "wrap_angle maps PI to -PI");
  check(near(math::wrap_angle(3.0f * PI), -PI),
        "wrap_angle wraps multiples of PI");
  check(near(math::wrap_angle(0.25f), 0.25f),
        "wrap_angle leaves in-range angles alone");
  check(near(math::wrap_angle(-1.5f * PI), 0.5f * PI),
        "wrap_angle wraps negative angles into range");

  check(near(math::lerp_angle(3.0f, -3.0f, 0.5f), PI, 1e-4f),
        "lerp_angle takes the shortest arc across PI");
  check(near(math::lerp_angle(0.0f, 1.0f, 0.25f), 0.25f),
        "lerp_angle matches plain lerp away from the wrap");

  printf("math_noise_helpers_test: %d/%d checks passed\n", checks_passed,
         checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
