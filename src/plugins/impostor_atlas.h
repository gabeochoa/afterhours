#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "../developer.h"

namespace afterhours::impostor_atlas {

struct AtlasItem {
  std::string name;
  int width = 0;
  int height = 0;
};

struct AtlasPlacement {
  std::string name;
  int x = 0, y = 0, width = 0, height = 0;

  struct UV {
    float u0 = 0.f, v0 = 0.f, u1 = 0.f, v1 = 0.f;
  };

  [[nodiscard]] UV uv(int atlas_width, int atlas_height) const {
    if (atlas_width <= 0 || atlas_height <= 0) return {};
    float w = static_cast<float>(atlas_width);
    float h = static_cast<float>(atlas_height);
    return {x / w, y / h, (x + width) / w, (y + height) / h};
  }
};

struct AtlasLayout {
  int width = 0;
  int height = 0;
  std::vector<AtlasPlacement> placements;

  [[nodiscard]] const AtlasPlacement *find(const std::string &name) const {
    for (const AtlasPlacement &p : placements)
      if (p.name == name) return &p;
    return nullptr;
  }
};

inline std::optional<AtlasLayout> pack(const std::vector<AtlasItem> &items,
                                       int max_width, int max_height,
                                       int padding = 0) {
  if (max_width <= 0 || max_height <= 0 || padding < 0) return std::nullopt;
  std::vector<std::size_t> order(items.size());
  for (std::size_t i = 0; i < items.size(); i++) {
    if (items[i].width <= 0 || items[i].height <= 0 ||
        items[i].width > max_width || items[i].height > max_height)
      return std::nullopt;
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    const AtlasItem &ia = items[a], &ib = items[b];
    if (ia.height != ib.height) return ia.height > ib.height;
    if (ia.width != ib.width) return ia.width > ib.width;
    if (ia.name != ib.name) return ia.name < ib.name;
    return a < b;
  });

  AtlasLayout out;
  out.placements.resize(items.size());
  int x = 0, shelf_y = 0, shelf_h = 0;
  for (std::size_t idx : order) {
    const AtlasItem &it = items[idx];
    if (x > 0 && x + it.width > max_width) {
      shelf_y += shelf_h + padding;
      x = 0;
      shelf_h = 0;
    }
    if (shelf_y + it.height > max_height) return std::nullopt;
    out.placements[idx] = {it.name, x, shelf_y, it.width, it.height};
    out.width = std::max(out.width, x + it.width);
    out.height = std::max(out.height, shelf_y + it.height);
    x += it.width + padding;
    shelf_h = std::max(shelf_h, it.height);
  }
  return out;
}

struct BillboardBasis {
  Vector3Type right{}, up{}, forward{};
  float half_size = 0.f;
};

inline BillboardBasis billboard_basis(Vector3Type center, Vector3Type eye,
                                      float radius) {
  Vector3Type to{eye.x - center.x, eye.y - center.y, eye.z - center.z};
  float dist = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
  if (dist < 1e-6f || radius <= 0.f)
    return {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, radius};
  float inv_dist = 1.f / dist;
  Vector3Type forward{to.x * inv_dist, to.y * inv_dist, to.z * inv_dist};
  Vector3Type up_hint = std::fabs(forward.y) > 0.999f
                            ? Vector3Type{0.f, 0.f, 1.f}
                            : Vector3Type{0.f, 1.f, 0.f};
  Vector3Type raw_right{up_hint.y * forward.z - up_hint.z * forward.y,
                        up_hint.z * forward.x - up_hint.x * forward.z,
                        up_hint.x * forward.y - up_hint.y * forward.x};
  float right_len = std::sqrt(raw_right.x * raw_right.x +
                              raw_right.y * raw_right.y +
                              raw_right.z * raw_right.z);
  Vector3Type right{};
  if (right_len > 0.f) {
    float inv_right_len = 1.f / right_len;
    right = {raw_right.x * inv_right_len, raw_right.y * inv_right_len,
             raw_right.z * inv_right_len};
  }
  Vector3Type up{forward.y * right.z - forward.z * right.y,
                 forward.z * right.x - forward.x * right.z,
                 forward.x * right.y - forward.y * right.x};
  float grow = dist * dist - radius * radius;
  float half = radius * dist / std::sqrt(std::max(grow, 0.01f * radius * radius));
  return {right, up, forward, half};
}

inline Vector3Type quad_point(Vector3Type center, const BillboardBasis &basis,
                              float x, float z) {
  return {center.x + (basis.right.x * x + basis.up.x * -z) * basis.half_size,
          center.y + (basis.right.y * x + basis.up.y * -z) * basis.half_size,
          center.z + (basis.right.z * x + basis.up.z * -z) * basis.half_size};
}

}
