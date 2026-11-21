// terrain_steering_test.cpp
// Ground-aligned movement over a height sampler: flat projection is exact,
// a slope projects onto its plane, steep slopes are rejected, and ground
// following snaps y within max_step.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/terrain_steering.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours::terrain_steer;

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

int main() {
  HeightSampler flat = [](float, float) { return 0.f; };
  HeightSampler slope = [](float x, float) { return 0.5f * x; }; // 26.57 deg

  // Flat ground: normal is straight up, projection is exact.
  {
    Vec3 n = ground_normal(flat, 0.f, 0.f);
    check(near(n.x, 0.f) && near(n.y, 1.f) && near(n.z, 0.f),
          "flat normal is up");
    Vec3 v = align_velocity(vec(3.f, 4.f, 5.f), n);
    check(near(v.x, 3.f) && near(v.y, 0.f) && near(v.z, 5.f),
          "flat projection exact");
    check(near(slope_degrees(n), 0.f), "flat slope is 0");
    check(can_climb(n, 0.f), "flat climbable at limit 0");
  }

  // Slope (0.5 per unit x): normal and projected velocity sit in the plane.
  {
    Vec3 n = ground_normal(slope, 2.f, 0.f);
    check(near(n.x, -0.4472136f, 1e-5f) && near(n.y, 0.8944272f, 1e-5f),
          "slope normal tilts into the hill");
    check(near(slope_degrees(n), 26.565051f, 1e-3f), "slope angle");
    Vec3 v = align_velocity(vec(1.f, 0.f, 0.f), n);
    check(near(v.x, 0.8f) && near(v.y, 0.4f) && near(v.z, 0.f),
          "slope projection");
    check(near(dot(v, n), 0.f), "projected velocity is orthogonal to normal");
    check(can_climb(n, 30.f), "26 deg climbs under a 30 deg limit");
    check(!can_climb(n, 20.f), "26 deg rejected under a 20 deg limit");
    check(!can_climb(n, 0.f), "slope rejected at limit 0");

    // Walking the projected velocity up the ramp: next position tracks the
    // ground, so ground following holds without sinking or floating.
    Vec3 start = vec(2.f, 1.f, 0.f);
    Vec3 next = move_and_snap(start, v, 1.f, slope, 1.f);
    check(near(next.x, 2.8f) && near(next.z, 0.f), "move follows desired x/z");
    check(near(next.y, 1.4f), "move snaps y onto the slope");
  }

  // Snap behavior: within max_step follows the ground, beyond is clamped.
  {
    Vec3 p = vec(4.f, 1.5f, 0.f); // ground 2.0 at x=4
    Vec3 snapped = snap_to_ground(p, slope, 1.f);
    check(near(snapped.y, 2.f), "snap lands on the ground within max_step");
    Vec3 clamped = snap_to_ground(p, slope, 0.25f);
    check(near(clamped.y, 1.75f), "snap clamps to max_step above the ground");
    Vec3 below = snap_to_ground(vec(4.f, 3.f, 0.f), slope, 0.25f);
    check(near(below.y, 2.75f), "snap clamps to max_step below the ground");
    Vec3 flat_snap = snap_to_ground(vec(1.f, -0.5f, 1.f), flat, 10.f);
    check(near(flat_snap.y, 0.f), "snap onto flat ground");

    HeightSampler wall = [](float, float) { return 10.f; };
    Vec3 moved = move_and_snap(vec(0.f, 0.f, 0.f), vec(0.f, 0.f, 0.f), 1.f,
                               wall, 0.5f);
    check(near(moved.y, 0.5f), "move_and_snap clamps a big ground jump");
  }

  std::printf("terrain_steering_test: %d/%d checks passed\n", checks - failures,
              checks);
  return failures > 0 ? 1 : 0;
}
