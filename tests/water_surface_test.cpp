#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/water_surface.h>

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

static bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

int main() {
  constexpr float PI_F = 3.14159265f;

  WaterSurface flat;
  flat.base_height = 2.5f;
  check(near(flat.sample_height(3.f, -7.f, 1.25f), 2.5f), "flat height is base");
  Vector3Type fn = flat.sample_normal(3.f, -7.f, 1.25f);
  check(near(fn.x, 0.f) && near(fn.y, 1.f) && near(fn.z, 0.f), "flat normal is up");

  WaterSurface water;
  water.base_height = 1.f;
  water.add_wave({0.5f, 4.f, 1.f, 0.f, 0.7f, 0.3f, 0.f});
  water.add_wave({0.25f, 2.f, 0.f, 1.f, -1.1f, 0.f, 0.f});
  auto analytic = [&](float x, float z, float t) {
    return 1.f + 0.5f * std::sin(x * (2.f * PI_F / 4.f) + 0.3f + 0.7f * t) +
                  0.25f * std::sin(z * (2.f * PI_F / 2.f) - 1.1f * t);
  };
  for (auto [x, z, t] : std::array<std::array<float, 3>, 4>{{{0.f, 0.f, 0.f},
                                                              {1.5f, -2.f, 0.4f},
                                                              {-3.f, 5.f, 2.1f},
                                                              {0.25f, 0.75f, -0.9f}}}) {
    check(near(water.sample_height(x, z, t), analytic(x, z, t), 1e-3f),
          "height matches analytic sum");
  }

  WaterSurface scaled;
  scaled.add_wave({0.5f, 4.f, 2.f, 0.f, 0.f, 0.f, 0.f});
  WaterSurface unit;
  unit.add_wave({0.5f, 4.f, 1.f, 0.f, 0.f, 0.f, 0.f});
  check(near(scaled.sample_height(1.3f, 0.f, 0.f), unit.sample_height(1.3f, 0.f, 0.f)),
        "wave direction normalized");

  WaterSurface broken;
  broken.base_height = 1.f;
  broken.add_wave({10.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f});
  check(near(broken.sample_height(5.f, 5.f, 5.f), 1.f), "zero wavelength ignored");

  Vector3Type n = water.sample_normal(1.5f, -2.f, 0.4f);
  float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
  check(near(len, 1.f, 1e-5f), "normal is unit length");
  const float eps = 1e-3f;
  float gx = (water.sample_height(1.5f + eps, -2.f, 0.4f) -
              water.sample_height(1.5f - eps, -2.f, 0.4f)) / (2.f * eps);
  float gz = (water.sample_height(1.5f, -2.f + eps, 0.4f) -
              water.sample_height(1.5f, -2.f - eps, 0.4f)) / (2.f * eps);
  float inv = 1.f / std::sqrt(gx * gx + 1.f + gz * gz);
  check(near(n.x, -gx * inv, 1e-2f) && near(n.z, -gz * inv, 1e-2f),
        "normal matches finite-difference gradient");

  check(near(water.sample_displaced(1.f, 2.f, 0.3f).x, 1.f), "no steepness, no shift");
  WaterSurface steep;
  steep.add_wave({0.5f, 4.f, 1.f, 0.f, 0.f, 0.f, 2.f});
  Vector3Type d = steep.sample_displaced(0.f, 0.f, 0.f);
  check(near(d.x, 1.f, 1e-4f) && near(d.z, 0.f) && near(d.y, 0.f),
        "gerstner-lite shifts +X by Q*A at phase 0");

  std::array<Vector3Type, 4> offsets{{{2.f, 0.f, 0.f}, {-2.f, 0.f, 0.f},
                                      {0.f, 0.f, 2.f}, {0.f, 0.f, -2.f}}};
  check(near(floating_height(flat, 1.f, 1.f, 0.f, offsets), 2.5f), "float height on flat");
  float expect = (water.sample_height(3.5f, -2.f, 0.4f) + water.sample_height(-0.5f, -2.f, 0.4f) +
                  water.sample_height(1.5f, 0.f, 0.4f) + water.sample_height(1.5f, -4.f, 0.4f)) / 4.f;
  check(near(floating_height(water, 1.5f, -2.f, 0.4f, offsets), expect, 1e-4f),
        "float height is the point average");

  float target = water.sample_height(1.5f, -2.f, 0.4f);
  float y = target - 5.f;
  for (int i = 0; i < 200; i++) y = settle_to_surface(y, target, 0.05f, 2.f);
  check(near(y, target, 1e-3f), "buoyancy settles at the surface");
  float y2 = target + 5.f;
  for (int i = 0; i < 200; i++) y2 = settle_to_surface(y2, target, 0.05f, 2.f);
  check(near(y2, target, 1e-3f), "buoyancy settles from above too");

  WaterSurface slope_x;
  slope_x.add_wave({0.5f, 4.f, 1.f, 0.f, 0.f, 0.f, 0.f});
  FloatingPose pose_x = floating_pose(slope_x, {0.f, 0.f, 0.f}, 0.f, {});
  check(near(pose_x.roll, std::atan(0.5f * PI_F / 2.f), 1e-4f), "roll follows X slope");
  check(near(pose_x.pitch, 0.f), "pitch stays 0 on X slope");
  check(near(pose_x.position.y, 0.f), "pose height at centre");

  WaterSurface slope_z;
  slope_z.add_wave({0.5f, 4.f, 0.f, 1.f, 0.f, 0.f, 0.f});
  FloatingPose pose_z = floating_pose(slope_z, {0.f, 0.f, 0.f}, 0.f, {});
  check(near(pose_z.pitch, -std::atan(0.5f * PI_F / 2.f), 1e-4f), "pitch follows Z slope");
  check(near(pose_z.roll, 0.f), "roll stays 0 on Z slope");

  // The floating helpers accept any height/normal sampler, not just the
  // built-in sine WaterSurface: a tilted plane (not a sine sum) with a
  // stateful gain, shaped like a game's own weather-driven sea.
  struct TiltedSea {
    float slope_x = 0.25f;
    float slope_z = -0.5f;
    [[nodiscard]] float sample_height(float x, float z, float) const {
      return slope_x * x + slope_z * z + 1.f;
    }
    [[nodiscard]] Vector3Type sample_normal(float, float, float) const {
      float inv = 1.f / std::sqrt(slope_x * slope_x + 1.f + slope_z * slope_z);
      return {-slope_x * inv, inv, -slope_z * inv};
    }
  };
  TiltedSea sea;
  check(near(floating_height(sea, 2.f, 4.f, 0.f, {}), -0.5f),
        "floating_height works on a non-WaterSurface sampler");
  FloatingPose sea_pose = floating_pose(sea, {0.f, 0.f, 0.f}, 0.f, {});
  check(near(sea_pose.pitch, std::atan2(0.5f, 1.f), 1e-3f) &&
            near(sea_pose.roll, std::atan2(0.25f, 1.f), 1e-3f),
        "floating_pose derives pitch/roll from a custom sampler");

  printf("water_surface: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
