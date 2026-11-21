#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <vector>

namespace afterhours::collision3d {

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length_sq(Vec3 a) { return dot(a, a); }
inline float length(Vec3 a) { return std::sqrt(length_sq(a)); }
inline Vec3 normalize(Vec3 a) {
  float l = length(a);
  return l > 1e-8f ? a * (1.f / l) : Vec3{0, 0, 0};
}

inline float clampf(float v, float lo, float hi) {
  return std::max(lo, std::min(v, hi));
}

inline Vec3 closest_on_segment(Vec3 p, Vec3 a, Vec3 b) {
  Vec3 ab = b - a;
  float d = length_sq(ab);
  float t = d > 1e-12f ? clampf(dot(p - a, ab) / d, 0.f, 1.f) : 0.f;
  return a + ab * t;
}

using MaterialId = int;
constexpr MaterialId kNoMaterial = -1;

struct AABB { Vec3 min, max; MaterialId material = 0; };
struct Sphere { Vec3 center; float radius = 0.5f; MaterialId material = 0; };
struct Capsule { Vec3 center; float radius = 0.5f; float half_height = 0.5f; MaterialId material = 0; };
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
  Vec3 point, normal;
  MaterialId material = kNoMaterial;
  Kind kind = Kind::AABB;
  int index = -1;
};

struct Overlap { Kind kind; int index; MaterialId material; };

namespace detail {
inline bool ray_aabb(Vec3 o, Vec3 d, const AABB &b, float &t_out, Vec3 &n_out) {
  float tmin = 0.f, tmax = std::numeric_limits<float>::max();
  Vec3 n{0, 0, 0};
  const float oa[3] = {o.x, o.y, o.z}, da[3] = {d.x, d.y, d.z};
  const float mn[3] = {b.min.x, b.min.y, b.min.z}, mx[3] = {b.max.x, b.max.y, b.max.z};
  for (int i = 0; i < 3; i++) {
    if (std::fabs(da[i]) < 1e-8f) {
      if (oa[i] < mn[i] || oa[i] > mx[i]) return false;
    } else {
      float inv = 1.f / da[i], t1 = (mn[i] - oa[i]) * inv, t2 = (mx[i] - oa[i]) * inv;
      Vec3 face{0, 0, 0};
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
  n_out = (tmin == 0.f) ? normalize(d * -1.f) : n;
  return true;
}

inline bool ray_sphere(Vec3 o, Vec3 d, Vec3 c, float r, float &t_out, Vec3 &n_out) {
  Vec3 oc = o - c;
  float b = dot(oc, d), cc = dot(oc, oc) - r * r, disc = b * b - cc;
  if (disc < 0) return false;
  float t = -b - std::sqrt(disc);
  if (t < 0) t = -b + std::sqrt(disc);
  if (t < 0) return false;
  t_out = t;
  n_out = normalize(o + d * t - c);
  return true;
}

inline Vec3 capsule_a(const Capsule &c) { return {c.center.x, c.center.y - c.half_height, c.center.z}; }
inline Vec3 capsule_b(const Capsule &c) { return {c.center.x, c.center.y + c.half_height, c.center.z}; }
inline Vec3 closest_point_aabb(Vec3 p, const AABB &b) {
  return {clampf(p.x, b.min.x, b.max.x), clampf(p.y, b.min.y, b.max.y), clampf(p.z, b.min.z, b.max.z)};
}

inline bool point_in_aabb(Vec3 p, const AABB &b) {
  return p.x >= b.min.x && p.x <= b.max.x && p.y >= b.min.y && p.y <= b.max.y && p.z >= b.min.z && p.z <= b.max.z;
}

inline Vec3 hf_normal(const Heightfield &h, float x, float z) {
  float e = 0.1f;
  float dx = h.sample(x + e, z) - h.sample(x - e, z);
  float dz = h.sample(x, z + e) - h.sample(x, z - e);
  return normalize({-dx / (2 * e), 1.f, -dz / (2 * e)});
}
}

inline std::optional<Hit> raycast(const World &w, Vec3 origin, Vec3 dir, float max_dist = 1e4f) {
  float dl = length(dir);
  if (dl < 1e-8f || max_dist < 0) return std::nullopt;
  Vec3 d = dir * (1.f / dl);
  std::optional<Hit> best;
  auto consider = [&](float t, Vec3 n, MaterialId m, Kind k, int idx) {
    if (t < 0 || t > max_dist) return;
    if (!best || t < best->t) best = Hit{t, origin + d * t, normalize(n), m, k, idx};
  };
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    float t; Vec3 n;
    if (detail::ray_aabb(origin, d, w.aabbs[i], t, n)) consider(t, n, w.aabbs[i].material, Kind::AABB, i);
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    float t; Vec3 n;
    if (detail::ray_sphere(origin, d, w.spheres[i].center, w.spheres[i].radius, t, n))
      consider(t, n, w.spheres[i].material, Kind::Sphere, i);
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    Vec3 a = detail::capsule_a(c), b = detail::capsule_b(c);
    float t; Vec3 n;
    if (detail::ray_sphere(origin, d, a, c.radius, t, n)) consider(t, n, c.material, Kind::Capsule, i);
    if (detail::ray_sphere(origin, d, b, c.radius, t, n)) consider(t, n, c.material, Kind::Capsule, i);
    Vec3 oc{origin.x - c.center.x, 0, origin.z - c.center.z};
    Vec3 dc{d.x, 0, d.z};
    float A = dot(dc, dc);
    if (A > 1e-8f) {
      float B = dot(oc, dc), C = dot(oc, oc) - c.radius * c.radius, disc = B * B - A * C;
      if (disc >= 0) {
        float tc = (-B - std::sqrt(disc)) / A;
        float y = origin.y + d.y * tc;
        if (tc >= 0 && y >= a.y && y <= b.y) {
          Vec3 p = origin + d * tc;
          consider(tc, {p.x - c.center.x, 0, p.z - c.center.z}, c.material, Kind::Capsule, i);
        }
      }
    }
  }
  for (int i = 0; i < (int)w.heightfields.size(); i++) {
    const Heightfield &h = w.heightfields[i];
    if (!h.sample) continue;
    const int kSteps = 128;
    float step = max_dist / kSteps, prev_t = 0;
    float prev_f = origin.y - h.sample(origin.x, origin.z);
    for (int s = 1; s <= kSteps; s++) {
      float t = step * s;
      Vec3 p = origin + d * t;
      if (p.x < h.min_x || p.x > h.max_x || p.z < h.min_z || p.z > h.max_z) { prev_t = t; prev_f = p.y - h.sample(clampf(p.x, h.min_x, h.max_x), clampf(p.z, h.min_z, h.max_z)); continue; }
      float f = p.y - h.sample(p.x, p.z);
      if ((prev_f > 0 && f <= 0) || (prev_f < 0 && f >= 0)) {
        float lo = prev_t, hi = t;
        for (int it = 0; it < 16; it++) {
          float mid = (lo + hi) * 0.5f;
          Vec3 mp = origin + d * mid;
          (mp.y - h.sample(mp.x, mp.z) > 0) == (prev_f > 0) ? lo = mid : hi = mid;
        }
        Vec3 hp = origin + d * hi;
        consider(hi, detail::hf_normal(h, hp.x, hp.z), h.material, Kind::Heightfield, i);
        break;
      }
      prev_t = t; prev_f = f;
    }
  }
  return best;
}

inline std::vector<Overlap> overlap_aabb(const World &w, Vec3 qmin, Vec3 qmax) {
  std::vector<Overlap> out;
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    const AABB &b = w.aabbs[i];
    if (qmin.x <= b.max.x && qmax.x >= b.min.x && qmin.y <= b.max.y && qmax.y >= b.min.y && qmin.z <= b.max.z && qmax.z >= b.min.z)
      out.push_back({Kind::AABB, i, b.material});
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    const Sphere &s = w.spheres[i];
    Vec3 cp = detail::closest_point_aabb(s.center, {qmin, qmax, 0});
    if (length_sq(s.center - cp) <= s.radius * s.radius) out.push_back({Kind::Sphere, i, s.material});
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    AABB ex{{qmin.x - c.radius, qmin.y - c.radius, qmin.z - c.radius}, {qmax.x + c.radius, qmax.y + c.radius, qmax.z + c.radius}};
    Vec3 a = detail::capsule_a(c), b = detail::capsule_b(c);
    bool hit = detail::point_in_aabb(a, ex) || detail::point_in_aabb(b, ex);
    if (!hit) {
      Vec3 d = b - a; float t0 = 0, t1 = 1;
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
    float xs[3] = {clampf((qmin.x + qmax.x) * 0.5f, h.min_x, h.max_x), clampf(qmin.x, h.min_x, h.max_x), clampf(qmax.x, h.min_x, h.max_x)};
    float zs[3] = {clampf((qmin.z + qmax.z) * 0.5f, h.min_z, h.max_z), clampf(qmin.z, h.min_z, h.max_z), clampf(qmax.z, h.min_z, h.max_z)};
    bool hit = false;
    for (float x : xs) for (float z : zs) { float y = h.sample(x, z); if (y >= qmin.y && y <= qmax.y) hit = true; }
    if (hit) out.push_back({Kind::Heightfield, i, h.material});
  }
  return out;
}

inline std::vector<Overlap> overlap_sphere(const World &w, Vec3 center, float radius) {
  std::vector<Overlap> out;
  AABB q{{center.x - radius, center.y - radius, center.z - radius}, {center.x + radius, center.y + radius, center.z + radius}};
  for (int i = 0; i < (int)w.aabbs.size(); i++) {
    Vec3 cp = detail::closest_point_aabb(center, w.aabbs[i]);
    if (length_sq(center - cp) <= radius * radius) out.push_back({Kind::AABB, i, w.aabbs[i].material});
  }
  for (int i = 0; i < (int)w.spheres.size(); i++) {
    float rr = radius + w.spheres[i].radius;
    if (length_sq(center - w.spheres[i].center) <= rr * rr) out.push_back({Kind::Sphere, i, w.spheres[i].material});
  }
  for (int i = 0; i < (int)w.capsules.size(); i++) {
    const Capsule &c = w.capsules[i];
    Vec3 cp = closest_on_segment(center, detail::capsule_a(c), detail::capsule_b(c));
    float rr = radius + c.radius;
    if (length_sq(center - cp) <= rr * rr) out.push_back({Kind::Capsule, i, c.material});
  }
  for (int i = 0; i < (int)w.heightfields.size(); i++) {
    const Heightfield &h = w.heightfields[i];
    if (!h.sample) continue;
    float cx = clampf(center.x, h.min_x, h.max_x), cz = clampf(center.z, h.min_z, h.max_z);
    float dx = center.x - cx, dz = center.z - cz;
    if (dx * dx + dz * dz > radius * radius) continue;
    if (center.y - radius <= h.sample(cx, cz)) out.push_back({Kind::Heightfield, i, h.material});
  }
  (void)q;
  return out;
}

inline Vec3 push_out_sphere(const World &w, Vec3 center, float radius) {
  Vec3 best{0, 0, 0};
  float best_len = 0;
  auto consider = [&](Vec3 push) {
    float l = length_sq(push);
    if (l > best_len) { best_len = l; best = push; }
  };
  for (const AABB &b : w.aabbs) {
    if (detail::point_in_aabb(center, b)) {
      float dxl = center.x - b.min.x, dxr = b.max.x - center.x;
      float dyl = center.y - b.min.y, dyr = b.max.y - center.y;
      float dzl = center.z - b.min.z, dzr = b.max.z - center.z;
      float m = dxl; Vec3 push{-(dxl + radius), 0, 0};
      if (dxr < m) { m = dxr; push = {dxr + radius, 0, 0}; }
      if (dyl < m) { m = dyl; push = {0, -(dyl + radius), 0}; }
      if (dyr < m) { m = dyr; push = {0, dyr + radius, 0}; }
      if (dzl < m) { m = dzl; push = {0, 0, -(dzl + radius)}; }
      if (dzr < m) { push = {0, 0, dzr + radius}; }
      consider(push);
    } else {
      Vec3 cp = detail::closest_point_aabb(center, b);
      Vec3 delta = center - cp;
      float dist = length(delta);
      if (dist < radius && dist > 1e-8f) consider(normalize(delta) * (radius - dist));
    }
  }
  for (const Sphere &s : w.spheres) {
    Vec3 delta = center - s.center;
    float dist = length(delta), rr = radius + s.radius;
    if (dist < rr) consider(dist > 1e-8f ? normalize(delta) * (rr - dist) : Vec3{0, rr, 0});
  }
  for (const Capsule &c : w.capsules) {
    Vec3 cp = closest_on_segment(center, detail::capsule_a(c), detail::capsule_b(c));
    Vec3 delta = center - cp;
    float dist = length(delta), rr = radius + c.radius;
    if (dist < rr) consider(dist > 1e-8f ? normalize(delta) * (rr - dist) : Vec3{0, rr, 0});
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
