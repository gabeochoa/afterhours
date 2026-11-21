#pragma once

// Water surface evaluation: pure math, no rendering or physics coupling.
//
//   WaterSurface water;  // base_height + up to 4 sum-of-sines waves
//   float h = water.sample_height(x, z, t);
//   Vector3Type n = water.sample_normal(x, z, t);  // unit, analytic
//   FloatingPose pose = floating_pose(water, center, t, offsets);
//
// Height is a sum of sines, so the normal is the closed-form gradient,
// not a finite difference. Rendering (meshes, shaders) and dynamics
// (forces, integration) live elsewhere; this only answers "how high is
// the water here, which way does it tilt, and where does a floater sit".

#include <cmath>
#include <cstddef>
#include <span>

#include "../developer.h"

namespace afterhours {

// One sine component. `speed` is angular frequency in rad/s, so the phase
// is dot(dir, p) * (2*pi / wavelength) + phase + speed * t.
struct WaterWave {
  float amplitude = 0.f;
  float wavelength = 1.f; // <= 0: the wave is ignored
  float dir_x = 1.f;      // travel direction on the XZ plane, normalized here
  float dir_z = 0.f;
  float speed = 0.f;
  float phase = 0.f;
  float steepness = 0.f; // Gerstner-lite horizontal shift; 0 = pure sine
};

struct WaterSurface {
  static constexpr int kMaxWaves = 4;

  float base_height = 0.f;
  WaterWave waves[kMaxWaves]{};
  int wave_count = 0;

  void add_wave(const WaterWave &wave) {
    if (wave_count < kMaxWaves) waves[wave_count++] = wave;
  }

  // Phase of one wave at (x, z, t), with its direction normalized.
  static float wave_phase(const WaterWave &w, float x, float z, float t) {
    float len = std::sqrt(w.dir_x * w.dir_x + w.dir_z * w.dir_z);
    if (len < 1e-6f) return w.phase + w.speed * t;
    float k = 2.f * 3.14159265f / w.wavelength;
    return ((w.dir_x / len) * x + (w.dir_z / len) * z) * k + w.phase + w.speed * t;
  }

  [[nodiscard]] float sample_height(float x, float z, float t) const {
    float h = base_height;
    int n = wave_count < 0 ? 0 : (wave_count > kMaxWaves ? kMaxWaves : wave_count);
    for (int i = 0; i < n; i++) {
      const WaterWave &w = waves[i];
      if (w.wavelength <= 0.f || w.amplitude == 0.f) continue;
      h += w.amplitude * std::sin(wave_phase(w, x, z, t));
    }
    return h;
  }

  // Closed-form gradient: dh/dx and dh/dz of the same sine sum.
  [[nodiscard]] Vector3Type sample_normal(float x, float z, float t) const {
    float gx = 0.f, gz = 0.f;
    int n = wave_count < 0 ? 0 : (wave_count > kMaxWaves ? kMaxWaves : wave_count);
    for (int i = 0; i < n; i++) {
      const WaterWave &w = waves[i];
      if (w.wavelength <= 0.f || w.amplitude == 0.f) continue;
      float len = std::sqrt(w.dir_x * w.dir_x + w.dir_z * w.dir_z);
      float dx = len < 1e-6f ? 0.f : w.dir_x / len;
      float dz = len < 1e-6f ? 0.f : w.dir_z / len;
      float k = 2.f * 3.14159265f / w.wavelength;
      float d = w.amplitude * k * std::cos(wave_phase(w, x, z, t));
      gx += d * dx;
      gz += d * dz;
    }
    float inv = 1.f / std::sqrt(gx * gx + 1.f + gz * gz);
    return {-gx * inv, inv, -gz * inv};
  }

  // Gerstner-lite displaced point: the sine height, plus a horizontal
  // shift along each wave's direction scaled by its steepness. The shift
  // never changes the height, so sample_height stays the analytic sum.
  [[nodiscard]] Vector3Type sample_displaced(float x, float z, float t) const {
    float px = x, pz = z;
    int n = wave_count < 0 ? 0 : (wave_count > kMaxWaves ? kMaxWaves : wave_count);
    for (int i = 0; i < n; i++) {
      const WaterWave &w = waves[i];
      if (w.wavelength <= 0.f || w.amplitude == 0.f || w.steepness == 0.f) continue;
      float len = std::sqrt(w.dir_x * w.dir_x + w.dir_z * w.dir_z);
      if (len < 1e-6f) continue;
      float shift = w.steepness * w.amplitude * std::cos(wave_phase(w, x, z, t));
      px += (w.dir_x / len) * shift;
      pz += (w.dir_z / len) * shift;
    }
    return {px, sample_height(x, z, t), pz};
  }
};

// Where a floating object sits on the surface: its centre height, its
// up vector (the average surface normal), and pitch/roll in radians,
// defined as the X/Z rotations that align +Y with that up vector.
struct FloatingPose {
  Vector3Type position{};
  Vector3Type up{0.f, 1.f, 0.f};
  float pitch = 0.f; // atan2(up.z, up.y)
  float roll = 0.f;  // atan2(-up.x, up.y)
};

// Average water height over buoyancy sample points (offsets from the
// centre, only x/z used). No offsets samples the centre alone.
[[nodiscard]] inline float floating_height(const WaterSurface &water, float cx,
                                           float cz, float t,
                                           std::span<const Vector3Type> offsets) {
  if (offsets.empty()) return water.sample_height(cx, cz, t);
  float sum = 0.f;
  for (Vector3Type o : offsets) sum += water.sample_height(cx + o.x, cz + o.z, t);
  return sum / static_cast<float>(offsets.size());
}

[[nodiscard]] inline Vector3Type floating_up(const WaterSurface &water, float cx,
                                             float cz, float t,
                                             std::span<const Vector3Type> offsets) {
  if (offsets.empty()) return water.sample_normal(cx, cz, t);
  Vector3Type sum{0.f, 0.f, 0.f};
  for (Vector3Type o : offsets) {
    Vector3Type n = water.sample_normal(cx + o.x, cz + o.z, t);
    sum.x += n.x;
    sum.y += n.y;
    sum.z += n.z;
  }
  float inv = 1.f / std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
  return {sum.x * inv, sum.y * inv, sum.z * inv};
}

[[nodiscard]] inline FloatingPose floating_pose(const WaterSurface &water,
                                                Vector3Type center, float t,
                                                std::span<const Vector3Type> offsets) {
  FloatingPose pose;
  pose.position = {center.x,
                   floating_height(water, center.x, center.z, t, offsets),
                   center.z};
  pose.up = floating_up(water, center.x, center.z, t, offsets);
  pose.pitch = std::atan2(pose.up.z, pose.up.y);
  pose.roll = std::atan2(-pose.up.x, pose.up.y);
  return pose;
}

// One damped step toward the surface height: y eases to the target with
// `rate` (1/s), so repeated steps settle instead of snapping.
[[nodiscard]] inline float settle_to_surface(float y, float target, float dt,
                                             float rate) {
  return target + (y - target) * std::exp(-rate * dt);
}

} // namespace afterhours
