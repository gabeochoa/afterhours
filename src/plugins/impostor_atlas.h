#pragma once

// Data side of house_of_cards atlas mode: pack many models' baked cards
// into one atlas so every far unique model draws in a single instanced
// call, and the camera-facing quad math that call needs on the CPU.
//
//   auto layout = impostor_atlas::pack({{"tree", 1024, 1024},
//                                       {"house", 512, 512}}, 4096, 4096);
//   // layout->placements[i] is the pixel rect for items[i]; .uv(w, h)
//   // gives normalised coords for the shader.
//   auto basis = impostor_atlas::billboard_basis(center, eye, radius);
//   auto corner = impostor_atlas::quad_point(center, basis, 1.f, -1.f);
//
// Renderer integration (blitting Baked images into one texture, an atlas
// shader variant, draw_atlas) is deliberately not here. No backend calls,
// so this header works on every backend. The billboard math mirrors
// CARD_VS in house_of_cards.h so a CPU copy and the shader agree.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace afterhours::impostor_atlas {

struct Vec3 {
  float x = 0.f, y = 0.f, z = 0.f;
};

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
  // v grows downward with y, matching image/texture upload order.
  [[nodiscard]] UV uv(int atlas_width, int atlas_height) const {
    if (atlas_width <= 0 || atlas_height <= 0) return {};
    float w = static_cast<float>(atlas_width);
    float h = static_cast<float>(atlas_height);
    return {x / w, y / h, (x + width) / w, (y + height) / h};
  }
};

struct AtlasLayout {
  int width = 0;  // used extent, not the requested maximum
  int height = 0;
  std::vector<AtlasPlacement> placements; // aligned with the input items

  [[nodiscard]] const AtlasPlacement *find(const std::string &name) const {
    for (const AtlasPlacement &p : placements)
      if (p.name == name) return &p;
    return nullptr;
  }
};

// Shelf packing, tallest first. Sorting by (height, width, name) makes the
// layout depend only on the item set, never on input order. Returns
// nullopt when any item is empty, larger than the atlas, or the shelves
// run past max_height -- a partial atlas is never handed back.
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
    if (x > 0 && x + it.width > max_width) { // next shelf
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
  Vec3 right{}, up{}, forward{}; // orthonormal; forward points at the eye
  float half_size = 0.f;         // quad half-extent covering the sphere
};

namespace detail {
inline Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
          a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) {
  return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}
inline Vec3 normalize(Vec3 a) {
  float len = length(a);
  return len > 0.f ? mul(a, 1.f / len) : Vec3{};
}
} // namespace detail

// Same construction as CARD_VS: up is world Y except near the poles, and
// half_size grows the quad to cover the bounding sphere's silhouette.
inline BillboardBasis billboard_basis(Vec3 center, Vec3 eye, float radius) {
  using namespace detail;
  Vec3 to = sub(eye, center);
  float dist = length(to);
  if (dist < 1e-6f || radius <= 0.f) // eye at the centre: any facing works
    return {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, radius};
  Vec3 forward = mul(to, 1.f / dist);
  Vec3 up_hint =
      std::fabs(forward.y) > 0.999f ? Vec3{0.f, 0.f, 1.f} : Vec3{0.f, 1.f, 0.f};
  Vec3 right = normalize(cross(up_hint, forward));
  Vec3 up = cross(forward, right);
  float grow = dist * dist - radius * radius;
  float half = radius * dist / std::sqrt(std::max(grow, 0.01f * radius * radius));
  return {right, up, forward, half};
}

// Point on the billboard quad for plane coords in [-1, 1]; x -> right,
// z -> -up, matching GenMeshPlane as CARD_VS consumes it.
inline Vec3 quad_point(Vec3 center, const BillboardBasis &basis, float x,
                       float z) {
  using namespace detail;
  Vec3 p = mul(basis.right, x);
  Vec3 q = mul(basis.up, -z);
  return {center.x + (p.x + q.x) * basis.half_size,
          center.y + (p.y + q.y) * basis.half_size,
          center.z + (p.z + q.z) * basis.half_size};
}

} // namespace afterhours::impostor_atlas
