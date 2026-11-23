#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "color.h"

namespace afterhours {

struct ZoneLighting {
  Color ambient{};
  Color fog{};
  float fog_near = 0.f;
  float fog_far = 0.f;
  float exposure = 1.f;
};

struct LightingZone {
  std::string name;
  Vector3Type min{};
  Vector3Type max{};
  ZoneLighting lighting{};
  float blend_width = 2.f;
};

[[nodiscard]] inline ZoneLighting lerp(const ZoneLighting &a,
                                       const ZoneLighting &b, float t) {
  auto c = [t](float x, float y) { return x + (y - x) * t; };
  return {colors::lerp(a.ambient, b.ambient, t),
          colors::lerp(a.fog, b.fog, t), c(a.fog_near, b.fog_near),
          c(a.fog_far, b.fog_far), c(a.exposure, b.exposure)};
}

struct LightingZoneSet {
  ZoneLighting default_lighting{};
  std::vector<LightingZone> zones;

  void add_zone(LightingZone zone) { zones.push_back(std::move(zone)); }

  [[nodiscard]] bool contains(const LightingZone &z, Vector3Type p) const {
    return p.x >= z.min.x && p.x <= z.max.x && p.y >= z.min.y &&
           p.y <= z.max.y && p.z >= z.min.z && p.z <= z.max.z;
  }

  [[nodiscard]] float signed_distance(const LightingZone &z,
                                      Vector3Type p) const {
    if (contains(z, p))
      return std::min({p.x - z.min.x, z.max.x - p.x, p.y - z.min.y,
                       z.max.y - p.y, p.z - z.min.z, z.max.z - p.z});
    const float dx = std::max({z.min.x - p.x, 0.f, p.x - z.max.x});
    const float dy = std::max({z.min.y - p.y, 0.f, p.y - z.max.y});
    const float dz = std::max({z.min.z - p.z, 0.f, p.z - z.max.z});
    return -std::sqrt(dx * dx + dy * dy + dz * dz);
  }

  [[nodiscard]] ZoneLighting sample(Vector3Type p) const {
    ZoneLighting sum{};
    float wsum = 0.f;
    for (const LightingZone &z : zones) {
      const float w = z.blend_width > 0.f
          ? std::clamp(0.5f + signed_distance(z, p) / z.blend_width, 0.f, 1.f)
          : (contains(z, p) ? 1.f : 0.f);
      if (w <= 0.f) continue;
      sum = lerp(sum, z.lighting, w / (wsum + w));
      wsum += w;
    }
    if (wsum <= 0.f) return default_lighting;
    return wsum >= 1.f ? sum : lerp(sum, default_lighting, 1.f - wsum);
  }
};

} // namespace afterhours
