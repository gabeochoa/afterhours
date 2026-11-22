#include <afterhours/src/core/math3d.h>

#include <cmath>
#include <cstdio>
#include <string>

static int checks_run = 0, checks_passed = 0;
static void check(bool c, const std::string &w) {
  checks_run++;
  if (c) checks_passed++; else fprintf(stderr, "  FAIL: %s\n", w.c_str());
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
static bool near3(afterhours::Vec3 a, afterhours::Vec3 b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}
static bool near_mat(const afterhours::Mat4 &a, const afterhours::Mat4 &b) {
  for (int i = 0; i < 16; i++) if (!near(a.m[i], b.m[i])) return false;
  return true;
}

int main() {
  using namespace afterhours;
  const float half_pi = 1.57079632679f;
  const float d = 0.70710678118f;

  check(near(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}), 32.f), "dot");
  check(near3(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}), {0, 0, 1}), "cross x*y=z");
  check(near3(cross(Vec3{0, 1, 0}, Vec3{1, 0, 0}), {0, 0, -1}), "cross order");
  check(near3(lerp(Vec3{0, 0, 0}, Vec3{2, 4, 6}, 0.5f), {1, 2, 3}), "lerp midpoint");

  check(near3(rotate(Quat::axis_angle({0, 0, 1}, half_pi), {1, 0, 0}), {0, 1, 0}),
        "axis-angle rotate z90");
  check(near3(rotate(Quat::axis_angle({0, 0, 5}, half_pi), {1, 0, 0}), {0, 1, 0}),
        "axis-angle normalizes axis");
  check(near3(rotate(Quat::identity(), {1, 2, 3}), {1, 2, 3}), "identity rotate");

  {
    Quat a = Quat::identity(), b = Quat::axis_angle({0, 0, 1}, half_pi);
    check(near3(rotate(slerp(a, b, 0.f), {1, 0, 0}), {1, 0, 0}), "slerp t=0");
    check(near3(rotate(slerp(a, b, 1.f), {1, 0, 0}), {0, 1, 0}), "slerp t=1");
    check(near3(rotate(slerp(a, b, 0.5f), {1, 0, 0}), {d, d, 0}), "slerp midpoint");
  }
  {
    Quat z90 = Quat::axis_angle({0, 0, 1}, half_pi);
    Quat x90 = Quat::axis_angle({1, 0, 0}, half_pi);
    // (z90 * x90) applies x90 first, then z90: (0,1,0) -> (0,0,1) -> (0,0,1)
    check(near3(rotate(z90 * x90, {0, 1, 0}), {0, 0, 1}), "quat multiply order a");
    // (x90 * z90) applies z90 first: (0,1,0) -> (-1,0,0) -> (-1,0,0)
    check(near3(rotate(x90 * z90, {0, 1, 0}), {-1, 0, 0}), "quat multiply order b");
    check(near3(rotate(z90 * z90, {1, 0, 0}), {-1, 0, 0}), "quat multiply composes");
  }
  {
    Mat4 t = to_mat4(TRS{{1, 2, 3}, Quat::identity(), {2, 2, 2}});
    check(near3(translation_of(t), {1, 2, 3}), "TRS translation");
    check(near3(transform_point(t, {1, 0, 0}), {3, 2, 3}), "TRS scale+translate");
    Mat4 r = to_mat4(TRS{{0, 0, 0}, Quat::axis_angle({0, 0, 1}, half_pi), {1, 1, 1}});
    check(near3(transform_point(r, {1, 0, 0}), {0, 1, 0}), "TRS rotate");
  }
  {
    Mat4 a = to_mat4(TRS{{1, 0, 0}, Quat::axis_angle({0, 0, 1}, half_pi), {1, 1, 1}});
    Mat4 b = to_mat4(TRS{{0, 1, 0}, Quat::identity(), {2, 1, 1}});
    check(near_mat(Mat4::identity() * a, a), "Mat4 identity left");
    check(near_mat(a * Mat4::identity(), a), "Mat4 identity right");
    check(near_mat((a * b) * a, a * (b * a)), "Mat4 multiply associativity");
    check(near3(transform_point(a * b, {0, 0, 0}),
                transform_point(a, transform_point(b, {0, 0, 0}))),
          "Mat4 multiply applies right first");
  }
  printf("%s: %d/%d checks passed\n", checks_passed == checks_run ? "PASS" : "FAIL",
         checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
