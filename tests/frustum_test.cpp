// frustum_test.cpp
// make_frustum against raylib's own projection. For random cameras and
// points, contains_point must agree with the clip-space test raylib's
// matrices give (-w <= x, y, z <= w). Spheres and boxes may report visible
// when they are not (the tests are conservative) but never the reverse, so
// any sample point inside them that projects on screen must mean visible.
//
// raymath only: no window needed.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>

namespace raylib {
#include <raylib.h>
// raymath zero-initialises with `= { 0 }`, which -Wextra flags once per use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/plugins/frustum.h>

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

static bool on_screen(const raylib::Matrix &vp, raylib::Vector3 p) {
  raylib::Vector4 c = raylib::QuaternionTransform({p.x, p.y, p.z, 1.f}, vp);
  return c.w > 0.f && c.x >= -c.w && c.x <= c.w && c.y >= -c.w &&
         c.y <= c.w && c.z >= -c.w && c.z <= c.w;
}

static raylib::Matrix view_projection(const Camera3D &cam, float aspect,
                                      float near_dist, float far_dist) {
  raylib::Matrix view = raylib::MatrixLookAt(cam.position, cam.target, cam.up);
  raylib::Matrix proj;
  if (cam.projection == camera::ORTHOGRAPHIC) {
    double top = cam.fovy / 2.0, right = top * aspect;
    proj = raylib::MatrixOrtho(-right, right, -top, top, near_dist, far_dist);
  } else {
    proj = raylib::MatrixPerspective(cam.fovy * DEG2RAD, aspect, near_dist,
                                     far_dist);
  }
  return raylib::MatrixMultiply(view, proj);
}

int main() {
  std::mt19937 rng(7);
  std::uniform_real_distribution<float> unit(-1.f, 1.f);
  auto rand_v = [&](float s) {
    return raylib::Vector3{unit(rng) * s, unit(rng) * s, unit(rng) * s};
  };

  // A camera at the origin looking down -Z, the obvious cases.
  {
    Camera3D cam{};
    cam.position = {0, 0, 0};
    cam.target = {0, 0, -1};
    cam.up = {0, 1, 0};
    cam.fovy = 60.f;
    cam.projection = camera::PERSPECTIVE;
    Frustum f = make_frustum(cam, 16.f / 9.f, 0.1f, 100.f);
    check(f.contains_point({0, 0, -10}), "point ahead is inside");
    check(!f.contains_point({0, 0, 10}), "point behind is outside");
    check(!f.contains_point({0, 0, -200}), "point past far is outside");
    check(!f.contains_point({0, 0, -0.05f}), "point before near is outside");
    check(!f.contains_point({0, 20, -10}), "point above the view is outside");
    check(f.intersects_sphere({0, 7, -10}, 2.f),
          "sphere straddling the top plane is visible");
    check(!f.intersects_sphere({0, 0, 10}, 2.f), "sphere behind is hidden");
    check(f.intersects_box({-1, -1, -11}, {1, 1, -9}), "box ahead is visible");
    check(!f.intersects_box({-1, -1, 9}, {1, 1, 11}), "box behind is hidden");
  }

  // Random cameras, both projections, against raylib's matrices.
  int point_mismatch = 0, sphere_miss = 0, box_miss = 0;
  for (int c = 0; c < 200; c++) {
    Camera3D cam{};
    cam.position = rand_v(20.f);
    cam.target = raylib::Vector3Add(cam.position, rand_v(1.f));
    cam.up = rand_v(1.f); // deliberately not perpendicular to the view
    bool ortho = c % 2 == 1;
    cam.projection = ortho ? camera::ORTHOGRAPHIC : camera::PERSPECTIVE;
    cam.fovy = ortho ? 5.f + 20.f * (unit(rng) + 1.f) : 30.f + 30.f * (unit(rng) + 1.f);
    float aspect = 0.5f + (unit(rng) + 1.f);
    float near_dist = 0.01f, far_dist = 60.f;
    Frustum f = make_frustum(cam, aspect, near_dist, far_dist);
    raylib::Matrix vp = view_projection(cam, aspect, near_dist, far_dist);

    for (int i = 0; i < 300; i++) {
      raylib::Vector3 p = raylib::Vector3Add(cam.position, rand_v(50.f));
      if (f.contains_point(p) != on_screen(vp, p)) {
        // Rounding can split the two on a point sitting on a plane, and
        // raylib's clip depth is coarse near the far plane (z/w is close to
        // 1 there): a point 0.01 past far at 60 still clips inside. Only
        // count points clear of every plane by a thousandth of the far
        // distance.
        float nearest = 1e9f;
        for (const Frustum::Plane &pl : f.planes)
          nearest = std::fmin(nearest, std::fabs(pl.distance(p.x, p.y, p.z)));
        if (nearest > 1e-3f * far_dist) point_mismatch++;
      }

      float r = 0.5f + 3.f * (unit(rng) + 1.f);
      bool sphere_seen = false;
      for (int s = 0; s < 24 && !sphere_seen; s++) {
        raylib::Vector3 q = raylib::Vector3Add(
            p, raylib::Vector3Scale(raylib::Vector3Normalize(rand_v(1.f)),
                                    r * (unit(rng) + 1.f) * 0.5f));
        sphere_seen = on_screen(vp, q);
      }
      if (sphere_seen && !f.intersects_sphere(p, r)) sphere_miss++;

      raylib::Vector3 half = {r, r * 0.5f, r * 1.5f};
      raylib::Vector3 lo = raylib::Vector3Subtract(p, half);
      raylib::Vector3 hi = raylib::Vector3Add(p, half);
      bool box_seen = false;
      for (int s = 0; s < 24 && !box_seen; s++) {
        raylib::Vector3 q = {lo.x + (hi.x - lo.x) * (unit(rng) + 1.f) * 0.5f,
                             lo.y + (hi.y - lo.y) * (unit(rng) + 1.f) * 0.5f,
                             lo.z + (hi.z - lo.z) * (unit(rng) + 1.f) * 0.5f};
        box_seen = on_screen(vp, q);
      }
      if (box_seen && !f.intersects_box(lo, hi)) box_miss++;
    }
  }
  printf("  points disagreeing with raylib: %d of 60000\n", point_mismatch);
  check(point_mismatch == 0, "contains_point matches raylib's projection");
  check(sphere_miss == 0, "no on-screen sphere reported hidden");
  check(box_miss == 0, "no on-screen box reported hidden");

  printf("frustum_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
