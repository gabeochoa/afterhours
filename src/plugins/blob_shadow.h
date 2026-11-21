#pragma once

// Blob shadows: a soft dark ellipse on the ground under an object.
// The directional ShadowMap pass is deferred (needs renderer integration
// decisions); this is the cheap stand-in games ship first.
//
//   auto blob = compute_blob_shadow(pos, 0.6f, 0.4f,
//                                   [](float x, float z) { return ground_y(x, z); });
//   draw_blob_shadow(blob);  // inside begin_3d/end_3d
//
// Height above the ground fades the blob and spreads it. Below-ground
// objects clamp to the ground (full strength, footprint size).

#include <algorithm>
#include <cmath>
#include <concepts>

#include "../drawing_helpers_3d.h"
#include "color.h"

namespace afterhours {

struct BlobShadowConfig {
  float base_alpha = 0.35f;    // alpha (0..1) at ground level
  float max_height = 8.f;      // height at/above which the blob is gone
  float radius_growth = 0.6f;  // fractional radius growth at max_height
  float ground_offset = 0.02f; // lift off the ground to avoid z-fighting
  Color color = {0, 0, 0, 255};
  int rings = 4; // concentric ellipses approximating a radial falloff
};

struct BlobShadow {
  Vector3Type center{}; // on the ground (+ ground_offset)
  float radius_x = 0.f;
  float radius_z = 0.f;
  float height = 0.f; // clamped height above the ground
  float alpha = 0.f;  // 0..1
  [[nodiscard]] bool visible() const {
    return alpha > 0.f && radius_x > 0.f && radius_z > 0.f;
  }
};

// ground_y is any callable float(float x, float z).
template <typename Ground>
  requires std::invocable<Ground, float, float> &&
           std::convertible_to<std::invoke_result_t<Ground, float, float>, float>
BlobShadow compute_blob_shadow(Vector3Type pos, float footprint_x,
                               float footprint_z, Ground &&ground_y,
                               const BlobShadowConfig &cfg = {}) {
  const float ground = ground_y(pos.x, pos.z);
  BlobShadow out;
  out.height = std::max(0.f, pos.y - ground);
  out.center = {pos.x, ground + cfg.ground_offset, pos.z};
  const float t = cfg.max_height > 0.f
                      ? std::clamp(out.height / cfg.max_height, 0.f, 1.f)
                      : (out.height > 0.f ? 1.f : 0.f);
  const float scale = 1.f + t * cfg.radius_growth;
  out.radius_x = std::max(0.f, footprint_x) * scale;
  out.radius_z = std::max(0.f, footprint_z) * scale;
  out.alpha = std::clamp(cfg.base_alpha, 0.f, 1.f) * (1.f - t);
  return out;
}

// Round footprint, and flat-ground convenience overloads.
template <typename Ground>
BlobShadow compute_blob_shadow(Vector3Type pos, float footprint_radius,
                               Ground &&ground_y,
                               const BlobShadowConfig &cfg = {}) {
  return compute_blob_shadow(pos, footprint_radius, footprint_radius, ground_y,
                             cfg);
}
inline BlobShadow compute_blob_shadow(Vector3Type pos, float footprint_x,
                                      float footprint_z, float ground_y,
                                      const BlobShadowConfig &cfg = {}) {
  return compute_blob_shadow(pos, footprint_x, footprint_z,
                             [ground_y](float, float) { return ground_y; },
                             cfg);
}
inline BlobShadow compute_blob_shadow(Vector3Type pos, float footprint_radius,
                                      float ground_y,
                                      const BlobShadowConfig &cfg = {}) {
  return compute_blob_shadow(pos, footprint_radius, footprint_radius, ground_y,
                             cfg);
}

inline void draw_blob_shadow(const BlobShadow &blob,
                             const BlobShadowConfig &cfg = {}) {
  if (!blob.visible()) return;
  const int rings = std::max(1, cfg.rings);
  // Each ring adds alpha/rings, so the centre accumulates to blob.alpha
  // and only the faintest ring reaches the edge: a radial falloff from
  // flat ellipse draws, no gradient primitive needed.
  const auto a = static_cast<unsigned char>(
      std::clamp(blob.alpha / static_cast<float>(rings), 0.f, 1.f) * 255.f);
  if (a == 0) return;
  const Color c = colors::set_opacity(cfg.color, a);
  for (int i = rings; i >= 1; --i) {
    const float f = static_cast<float>(i) / static_cast<float>(rings);
    draw_ellipse_3d(blob.center, blob.radius_x * f, blob.radius_z * f, c);
  }
}

template <typename Ground>
void draw_blob_shadow(Vector3Type pos, float footprint_x, float footprint_z,
                      Ground &&ground_y, const BlobShadowConfig &cfg = {}) {
  draw_blob_shadow(
      compute_blob_shadow(pos, footprint_x, footprint_z, ground_y, cfg), cfg);
}

} // namespace afterhours
