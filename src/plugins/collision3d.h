#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

#include "../developer.h"

namespace afterhours::collision3d {

inline Vector3Type closest_on_segment(Vector3Type p, Vector3Type a, Vector3Type b) {
  float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
  float d = abx * abx + aby * aby + abz * abz;
  float t = d > 1e-12f ? std::clamp(((p.x - a.x) * abx + (p.y - a.y) * aby + (p.z - a.z) * abz) / d, 0.f, 1.f) : 0.f;
  return {a.x + abx * t, a.y + aby * t, a.z + abz * t};
}

using MaterialId = int;
constexpr MaterialId NO_MATERIAL = -1;

struct AABB { Vector3Type min{}, max{}; MaterialId material = 0; };
struct Sphere { Vector3Type center{}; float radius = 0.5f; MaterialId material = 0; };
struct Capsule { Vector3Type center{}; float radius = 0.5f; float half_height = 0.5f; MaterialId material = 0; };
struct Heightfield {
  std::function<float(float, float)> sample;
  float min_x = -1e4f, max_x = 1e4f, min_z = -1e4f, max_z = 1e4f;
  MaterialId material = 0;
};

enum class Kind { AABB, Sphere, Capsule, Heightfield };

struct World {
  std::vector<AABB> aabbs;
  std::vector<Sphere> spheres;
  std::vector<Capsule> capsules;
  std::vector<Heightfield> heightfields;
  void add(AABB s) { aabbs.push_back(s); }
  void add(Sphere s) { spheres.push_back(s); }
  void add(Capsule s) { capsules.push_back(s); }
  void add(Heightfield s) { heightfields.push_back(std::move(s)); }
  void clear() { aabbs.clear(); spheres.clear(); capsules.clear(); heightfields.clear(); }
  [[nodiscard]] bool empty() const {
    return aabbs.empty() && spheres.empty() && capsules.empty() && heightfields.empty();
  }
};

struct Hit {
  float t = 0;
  Vector3Type point{}, normal{};
  MaterialId material = NO_MATERIAL;
  Kind kind = Kind::AABB;
  int index = -1;
};

struct Overlap { Kind kind; int index; MaterialId material; };

namespace detail {
inline bool ray_aabb(Vector3Type o, Vector3Type d, const AABB &b, float &t_out, Vector3Type &n_out) {
  float tmin = 0.f, tmax = std::numeric_limits<float>::max();
  Vector3Type n{0, 0, 0};
  const float oa[3] = {o.x, o.y, o.z}, da[3] = {d.x, d.y, d.z};
  const float mn[3] = {b.min.x, b.min.y, b.min.z}, mx[3] = {b.max.x, b.max.y, b.max.z};
  for (int i = 0; i < 3; i++) {
    if (std::fabs(da[i]) < 1e-8f) {
      if (oa[i] < mn[i] || oa[i] > mx[i]) return false;
    } else {
      float inv = 1.f / da[i], t1 = (mn[i] - oa[i]) * inv, t2 = (mx[i] - oa[i]) * inv;
      Vector3Type face{0, 0, 0};
      if (i == 0) face.x = da[i] > 0 ? -1.f : 1.f;
      if (i == 1) face.y = da[i] > 0 ? -1.f : 1.f;
      if (i == 2) face.z = da[i] > 0 ? -1.f : 1.f;
      if (t1 > t2) std::swap(t1, t2);
      if (t1 > tmin) { tmin = t1; n = face; }
      tmax = std::min(tmax, t2);
      if (tmin > tmax) return false;
    }
  }
  t_out = tmin;
  if (tmin == 0.f) {
    float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    n_out = l > 1e-8f ? Vector3Type{(d.x * -1.f) * (1.f / l), (d.y * -1.f) * (1.f / l), (d.z * -1.f) * (1.f / l)} : Vector3Type{0, 0, 0};
  } else {
    n_out = n;
  }
  return true;
}

inline bool ray_sphere(Vector3Type o, Vector3Type d, Vector3Type c, float r, float &t_out, Vector3Type &n_out) {
  float ocx = o.x - c.x, ocy = o.y - c.y, ocz = o.z - c.z;
  float b = ocx * d.x + ocy * d.y + ocz * d.z, cc = ocx * ocx + ocy * ocy + ocz * ocz - r * r, disc = b * b - cc;
  if (disc < 0) return false;
  float t = -b - std::sqrt(disc);
  if (t < 0) t = -b + std::sqrt(disc);
  if (t < 0) return false;
  t_out = t;
  float nx = o.x + d.x * t - c.x, ny = o.y + d.y * t - c.y, nz = o.z + d.z * t - c.z;
  float l = std::sqrt(nx * nx + ny * ny + nz * nz);
  n_out = l > 1e-8f ? Vector3Type{nx * (1.f / l), ny * (1.f / l), nz * (1.f / l)} : Vector3Type{0, 0, 0};
  return true;
}

inline Vector3Type capsule_a(const Capsule &c) { return {c.center.x, c.center.y - c.half_height, c.center.z}; }

inline Vector3Type capsule_b(const Capsule &c) { return {c.center.x, c.center.y + c.half_height, c.center.z}; }

inline Vector3Type closest_point_aabb(Vector3Type p, const AABB &b) {
  return {std::clamp(p.x, b.min.x, b.max.x), std::clamp(p.y, b.min.y, b.max.y), std::clamp(p.z, b.min.z, b.max.z)};
}

inline bool point_in_aabb(Vector3Type p, const AABB &b) {
  return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
}

inline Vector3Type hf_normal(const Heightfield &h, float x, float z) {
  float e = 0.1f;
  float dx = h.sample(x + e, z) - h.sample(x - e, z);
  float dz = h.sample(x, z + e) - h.sample(x, z - e);
  float vx = -dx / (2 * e), vy = 1.f, vz = -dz / (2 * e);
  float l = std::sqrt(vx * vx + vy * vy + vz * vz);
  return l > 1e-8f ? Vector3Type{vx * (1.f / l), vy * (1.f / l), vz * (1.f / l)} : Vector3Type{0, 0, 0};
}
}

inline std::optional<Hit> raycast(const World &w, Vector3Type origin, Vector3Type dir, float max_dist = 1e4f) {
  float dl = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
  if (dl < 1e-8f || max_dist < 0) return std::nullopt;
  Vector3Type d{dir.x * (1.f / dl), dir.y * (1.f / dl), dir.z * (1.f / dl)};
  std::optional<Hit> best;
  auto consider = [&](float t, Vector3Type n, MaterialId m, Kind k, int idx) {
    if (t < 0 || t > max_dist) return;
    if (!best || t < best->t) {
      float nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
      Vector3Type nn = nl > 1e-8f ? Vector3Type{n.x * (1.f / nl), n.y * (1.f / nl), n.z * (1.f / nl)} : Vector3Type{0, 0, 0};
      best = Hit{t, {origin.x + d.x * t, origin.y + d.y * t, origin.z + d.z * t}, nn, m, k, idx};
    }
  };
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    float t; Vector3Type n;
    if (detail::ray_aabb(origin, d, w.aabbs[i], t, n)) consider(t, n, w.aabbs[i].material, Kind::AABB, i);
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    float t; Vector3Type n;
    if (detail::ray_sphere(origin, d, w.spheres[i].center, w.spheres[i].radius, t, n))
      consider(t, n, w.spheres[i].material, Kind::Sphere, i);
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    Vector3Type a = detail::capsule_a(c), b = detail::capsule_b(c);
    float t; Vector3Type n;
    if (detail::ray_sphere(origin, d, a, c.radius, t, n)) consider(t, n, c.material, Kind::Capsule, i);
    if (detail::ray_sphere(origin, d, b, c.radius, t, n)) consider(t, n, c.material, Kind::Capsule, i);
    Vector3Type oc{origin.x - c.center.x, 0, origin.z - c.center.z};
    Vector3Type dc{d.x, 0, d.z};
    float A = dc.x * dc.x + dc.z * dc.z;
    if (A > 1e-8f) {
      float B = oc.x * dc.x + oc.z * dc.z, C = oc.x * oc.x + oc.z * oc.z - c.radius * c.radius, disc = B * B - A * C;
      if (disc >= 0) {
        float tc = (-B - std::sqrt(disc)) / A;
        float y = origin.y + d.y * tc;
        if (tc >= 0 && y >= a.y && y <= b.y) {
          Vector3Type p{origin.x + d.x * tc, origin.y + d.y * tc, origin.z + d.z * tc};
          consider(tc, {p.x - c.center.x, 0, p.z - c.center.z}, c.material, Kind::Capsule, i);
        }
      }
    }
  }
  for (int i = 0; i < (int)w.heightfields.size(); i++) {
    const Heightfield &h = w.heightfields[i];
    if (!h.sample) continue;
    const int STEPS = 128;
    float step = max_dist / STEPS, prev_t = 0;
    float prev_f = origin.y - h.sample(origin.x, origin.z);
    for (int s = 1; s <= STEPS; s++) {
      float t = step * s;
      Vector3Type p{origin.x + d.x * t, origin.y + d.y * t, origin.z + d.z * t};
      if (p.x < h.min_x || p.x > h.max_x || p.z < h.min_z || p.z > h.max_z) { prev_t = t; prev_f = p.y - h.sample(std::clamp(p.x, h.min_x, h.max_x), std::clamp(p.z, h.min_z, h.max_z)); continue; }
      float f = p.y - h.sample(p.x, p.z);
      if ((prev_f > 0 && f <= 0) || (prev_f < 0 && f >= 0)) {
        float lo = prev_t, hi = t;
        for (int it = 0; it < 16; it++) {
          float mid = (lo + hi) * 0.5f;
          Vector3Type mp{origin.x + d.x * mid, origin.y + d.y * mid, origin.z + d.z * mid};
          (mp.y - h.sample(mp.x, mp.z) > 0) == (prev_f > 0) ? lo = mid : hi = mid;
        }
        Vector3Type hp{origin.x + d.x * hi, origin.y + d.y * hi, origin.z + d.z * hi};
        consider(hi, detail::hf_normal(h, hp.x, hp.z), h.material, Kind::Heightfield, i);
        break;
      }
      prev_t = t; prev_f = f;
    }
  }
  return best;
}

inline std::vector<Overlap> overlap_aabb(const World &w, Vector3Type qmin, Vector3Type qmax) {
  std::vector<Overlap> out;
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    const AABB &b = w.aabbs[i];
    if (qmin.x <= b.max.x && qmax.x >= b.min.x && qmin.y <= b.max.y && qmax.y >= b.min.y && qmin.z <= b.max.z && qmax.z >= b.min.z)
      out.push_back({Kind::AABB, i, b.material});
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    const Sphere &s = w.spheres[i];
    Vector3Type cp = detail::closest_point_aabb(s.center, {qmin, qmax, 0});
    float dx = s.center.x - cp.x, dy = s.center.y - cp.y, dz = s.center.z - cp.z;
    if (dx * dx + dy * dy + dz * dz <= s.radius * s.radius) out.push_back({Kind::Sphere, i, s.material});
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    AABB ex{{qmin.x - c.radius, qmin.y - c.radius, qmin.z - c.radius}, {qmax.x + c.radius, qmax.y + c.radius, qmax.z + c.radius}};
    Vector3Type a = detail::capsule_a(c), b = detail::capsule_b(c);
    bool hit = detail::point_in_aabb(a, ex) || detail::point_in_aabb(b, ex);
    if (!hit) {
      Vector3Type d{b.x - a.x, b.y - a.y, b.z - a.z}; float t0 = 0, t1 = 1;
      const float pa[3] = {a.x, a.y, a.z}, da[3] = {d.x, d.y, d.z};
      const float mn[3] = {ex.min.x, ex.min.y, ex.min.z}, mx[3] = {ex.max.x, ex.max.y, ex.max.z};
      hit = true;
      for (int ax = 0; ax < 3; ax++) {
        if (std::fabs(da[ax]) < 1e-8f) { if (pa[ax] < mn[ax] || pa[ax] > mx[ax]) hit = false; }
        else {
          float inv = 1.f / da[ax], u = (mn[ax] - pa[ax]) * inv, v = (mx[ax] - pa[ax]) * inv;
          if (u > v) std::swap(u, v);
          t0 = std::max(t0, u); t1 = std::min(t1, v);
          if (t0 > t1) hit = false;
        }
      }
    }
    if (hit) out.push_back({Kind::Capsule, i, c.material});
  }
  for (int i = 0; i < (int)w.heightfields.size(); i++) {
    const Heightfield &h = w.heightfields[i];
    if (!h.sample || qmax.x < h.min_x || qmin.x > h.max_x || qmax.z < h.min_z || qmin.z > h.max_z) continue;
    float xs[3] = {std::clamp((qmin.x + qmax.x) * 0.5f, h.min_x, h.max_x), std::clamp(qmin.x, h.min_x, h.max_x), std::clamp(qmax.x, h.min_x, h.max_x)};
    float zs[3] = {std::clamp((qmin.z + qmax.z) * 0.5f, h.min_z, h.max_z), std::clamp(qmin.z, h.min_z, h.max_z), std::clamp(qmax.z, h.min_z, h.max_z)};
    bool hit = false;
    for (float x : xs) for (float z : zs) { float y = h.sample(x, z); if (y >= qmin.y && y <= qmax.y) hit = true; }
    if (hit) out.push_back({Kind::Heightfield, i, h.material});
  }
  return out;
}

inline std::vector<Overlap> overlap_sphere(const World &w, Vector3Type center, float radius) {
  std::vector<Overlap> out;
  AABB q{{center.x - radius, center.y - radius, center.z - radius}, {center.x + radius, center.y + radius, center.z + radius}};
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    Vector3Type cp = detail::closest_point_aabb(center, w.aabbs[i]);
    float dx = center.x - cp.x, dy = center.y - cp.y, dz = center.z - cp.z;
    if (dx * dx + dy * dy + dz * dz <= radius * radius) out.push_back({Kind::AABB, i, w.aabbs[i].material});
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    float rr = radius + w.spheres[i].radius;
    float dx = center.x - w.spheres[i].center.x, dy = center.y - w.spheres[i].center.y, dz = center.z - w.spheres[i].center.z;
    if (dx * dx + dy * dy + dz * dz <= rr * rr) out.push_back({Kind::Sphere, i, w.spheres[i].material});
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    Vector3Type cp = closest_on_segment(center, detail::capsule_a(c), detail::capsule_b(c));
    float rr = radius + c.radius;
    float dx = center.x - cp.x, dy = center.y - cp.y, dz = center.z - cp.z;
    if (dx * dx + dy * dy + dz * dz <= rr * rr) out.push_back({Kind::Capsule, i, c.material});
  }
  for (int i = 0; i < (int)w.heightfields.size(); i++) {
    const Heightfield &h = w.heightfields[i];
    if (!h.sample) continue;
    float cx = std::clamp(center.x, h.min_x, h.max_x), cz = std::clamp(center.z, h.min_z, h.max_z);
    float dx = center.x - cx, dz = center.z - cz;
    if (dx * dx + dz * dz > radius * radius) continue;
    if (center.y - radius <= h.sample(cx, cz)) out.push_back({Kind::Heightfield, i, h.material});
  }
  (void)q;
  return out;
}

inline Vector3Type push_out_sphere(const World &w, Vector3Type center, float radius) {
  Vector3Type best{0, 0, 0};
  float best_len = 0;
  auto consider = [&](Vector3Type push) {
    float l = push.x * push.x + push.y * push.y + push.z * push.z;
    if (l > best_len) { best_len = l; best = push; }
  };
  for (const AABB &b : w.aabbs) {
    if (detail::point_in_aabb(center, b)) {
      float dxl = center.x - b.min.x, dxr = b.max.x - center.x;
      float dyl = center.y - b.min.y, dyr = b.max.y - center.y;
      float dzl = center.z - b.min.z, dzr = b.max.z - center.z;
      float m = dxl; Vector3Type push{-(dxl + radius), 0, 0};
      if (dxr < m) { m = dxr; push = {dxr + radius, 0, 0}; }
      if (dyl < m) { m = dyl; push = {0, -(dyl + radius), 0}; }
      if (dyr < m) { m = dyr; push = {0, dyr + radius, 0}; }
      if (dzl < m) { m = dzl; push = {0, 0, -(dzl + radius)}; }
      if (dzr < m) { push = {0, 0, dzr + radius}; }
      consider(push);
    } else {
      Vector3Type cp = detail::closest_point_aabb(center, b);
      Vector3Type delta{center.x - cp.x, center.y - cp.y, center.z - cp.z};
      float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
      if (dist < radius && dist > 1e-8f) consider({(delta.x * (1.f / dist)) * (radius - dist), (delta.y * (1.f / dist)) * (radius - dist), (delta.z * (1.f / dist)) * (radius - dist)});
    }
  }
  for (const Sphere &s : w.spheres) {
    Vector3Type delta{center.x - s.center.x, center.y - s.center.y, center.z - s.center.z};
    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z), rr = radius + s.radius;
    if (dist < rr) consider(dist > 1e-8f ? Vector3Type{(delta.x * (1.f / dist)) * (rr - dist), (delta.y * (1.f / dist)) * (rr - dist), (delta.z * (1.f / dist)) * (rr - dist)} : Vector3Type{0, rr, 0});
  }
  for (const Capsule &c : w.capsules) {
    Vector3Type cp = closest_on_segment(center, detail::capsule_a(c), detail::capsule_b(c));
    Vector3Type delta{center.x - cp.x, center.y - cp.y, center.z - cp.z};
    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z), rr = radius + c.radius;
    if (dist < rr) consider(dist > 1e-8f ? Vector3Type{(delta.x * (1.f / dist)) * (rr - dist), (delta.y * (1.f / dist)) * (rr - dist), (delta.z * (1.f / dist)) * (rr - dist)} : Vector3Type{0, rr, 0});
  }
  for (const Heightfield &h : w.heightfields) {
    if (!h.sample || center.x < h.min_x || center.x > h.max_x || center.z < h.min_z || center.z > h.max_z) continue;
    float ground = h.sample(center.x, center.z);
    if (center.y - radius < ground) consider({0, ground + radius - center.y, 0});
  }
  return best;
}

inline std::optional<float> ground_height(const World &w, float x, float z) {
  std::optional<float> best;
  for (const Heightfield &h : w.heightfields) {
    if (!h.sample || x < h.min_x || x > h.max_x || z < h.min_z || z > h.max_z) continue;
    float y = h.sample(x, z);
    if (!best || y > *best) best = y;
  }
  return best;
}

}
