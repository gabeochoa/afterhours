#pragma once

// View frustum culling: is this point, sphere or box anywhere on screen?
//
//   Frustum view = make_frustum(camera, screen_w / screen_h);
//   if (!view.intersects_box(bounds.min, bounds.max)) continue;
//
// Built from the camera directly, so it works on every backend and needs no
// matrices; make_frustum(view_projection) builds it from a matrix instead. Perspective and orthographic both follow raylib's BeginMode3D:
// fovy is the vertical angle in degrees, or the view height in world units
// for orthographic. near / far default to raylib's cull distances.
//
// The sphere and box tests are conservative: something just outside a corner
// of the frustum can still report visible. Nothing visible reports hidden.

#include <cmath>

#include "../developer.h"
#include "../drawing_helpers_3d.h"

namespace afterhours {

struct Frustum {
  // Normals point inward: a point is inside a plane when
  // x * p.x + y * p.y + z * p.z + d >= 0.
  struct Plane {
    float x = 0.f, y = 0.f, z = 0.f, d = 0.f;
    [[nodiscard]] float distance(float px, float py, float pz) const {
      return x * px + y * py + z * pz + d;
    }
  };
  Plane planes[6]; // left, right, bottom, top, near, far

  [[nodiscard]] bool contains_point(Vector3Type p) const {
    for (const Plane &pl : planes)
      if (pl.distance(p.x, p.y, p.z) < 0.f) return false;
    return true;
  }

  [[nodiscard]] bool intersects_sphere(Vector3Type center, float radius) const {
    for (const Plane &pl : planes)
      if (pl.distance(center.x, center.y, center.z) < -radius) return false;
    return true;
  }

  // Axis-aligned box, e.g. raylib's GetModelBoundingBox moved to the copy.
  [[nodiscard]] bool intersects_box(Vector3Type min, Vector3Type max) const {
    for (const Plane &pl : planes) {
      // The corner furthest along the plane's normal; if even that one is
      // outside, the whole box is.
      float px = pl.x >= 0.f ? max.x : min.x;
      float py = pl.y >= 0.f ? max.y : min.y;
      float pz = pl.z >= 0.f ? max.z : min.z;
      if (pl.distance(px, py, pz) < 0.f) return false;
    }
    return true;
  }
};

namespace detail_frustum {
struct V {
  float x, y, z;
};
inline V sub(V a, V b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V add(V a, V b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V mul(V a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float dot(V a, V b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V cross(V a, V b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline V normalize(V a) { return mul(a, 1.f / std::sqrt(dot(a, a))); }
inline Frustum::Plane plane(V normal, V point) {
  return {normal.x, normal.y, normal.z, -dot(normal, point)};
}
} // namespace detail_frustum

inline Frustum make_frustum(const Camera3D &cam, float aspect,
                            float near_dist = 0.01f, float far_dist = 1000.f) {
  using namespace detail_frustum;
  V pos{cam.position.x, cam.position.y, cam.position.z};
  V f = normalize(sub(V{cam.target.x, cam.target.y, cam.target.z}, pos));
  V r = normalize(cross(f, V{cam.up.x, cam.up.y, cam.up.z}));
  V u = cross(r, f); // cam.up need not be perpendicular to the view

  Frustum out;
  if (cam.projection == camera::ORTHOGRAPHIC) {
    float hh = cam.fovy * 0.5f, hw = hh * aspect;
    out.planes[0] = plane(r, sub(pos, mul(r, hw)));
    out.planes[1] = plane(mul(r, -1.f), add(pos, mul(r, hw)));
    out.planes[2] = plane(u, sub(pos, mul(u, hh)));
    out.planes[3] = plane(mul(u, -1.f), add(pos, mul(u, hh)));
  } else {
    // Each side plane passes through the camera and holds one screen edge.
    // tan * f -/+ r and friends are its inward normals.
    float tv = std::tan(cam.fovy * 0.5f * DEG2RAD);
    float th = tv * aspect;
    out.planes[0] = plane(normalize(add(mul(f, th), r)), pos);
    out.planes[1] = plane(normalize(sub(mul(f, th), r)), pos);
    out.planes[2] = plane(normalize(add(mul(f, tv), u)), pos);
    out.planes[3] = plane(normalize(sub(mul(f, tv), u)), pos);
  }
  out.planes[4] = plane(f, add(pos, mul(f, near_dist)));
  out.planes[5] = plane(mul(f, -1.f), add(pos, mul(f, far_dist)));
  return out;
}

// From a combined view * projection matrix, e.g. raylib's
// MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()) inside the
// pass being culled, so a shadow pass or a picture-in-picture with its own
// projection and clip planes needs no camera. Any type with raylib's m0..m15
// fields works.
template <typename M>
  requires requires(const M &m) { m.m0 + m.m15; }
inline Frustum make_frustum(const M &vp) {
  Frustum out;
  auto set = [&](int i, float x, float y, float z, float d) {
    float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.f) {
      x /= len;
      y /= len;
      z /= len;
      d /= len;
    }
    out.planes[i] = {x, y, z, d};
  };
  set(0, vp.m3 + vp.m0, vp.m7 + vp.m4, vp.m11 + vp.m8, vp.m15 + vp.m12);
  set(1, vp.m3 - vp.m0, vp.m7 - vp.m4, vp.m11 - vp.m8, vp.m15 - vp.m12);
  set(2, vp.m3 + vp.m1, vp.m7 + vp.m5, vp.m11 + vp.m9, vp.m15 + vp.m13);
  set(3, vp.m3 - vp.m1, vp.m7 - vp.m5, vp.m11 - vp.m9, vp.m15 - vp.m13);
  set(4, vp.m3 + vp.m2, vp.m7 + vp.m6, vp.m11 + vp.m10, vp.m15 + vp.m14);
  set(5, vp.m3 - vp.m2, vp.m7 - vp.m6, vp.m11 - vp.m10, vp.m15 - vp.m14);
  return out;
}

} // namespace afterhours
