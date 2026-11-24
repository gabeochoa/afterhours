#include <afterhours/src/core/math3d.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

static_assert(std::is_same_v<afterhours::Vec3, raylib::Vector3>);

static_assert(std::is_same_v<afterhours::Quat, raylib::Quaternion>);

static_assert(std::is_same_v<afterhours::Mat4, raylib::Matrix>);

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
  return near(a.m0, b.m0) && near(a.m1, b.m1) && near(a.m2, b.m2) &&
         near(a.m3, b.m3) && near(a.m4, b.m4) && near(a.m5, b.m5) &&
         near(a.m6, b.m6) && near(a.m7, b.m7) && near(a.m8, b.m8) &&
         near(a.m9, b.m9) && near(a.m10, b.m10) && near(a.m11, b.m11) &&
         near(a.m12, b.m12) && near(a.m13, b.m13) && near(a.m14, b.m14) &&
         near(a.m15, b.m15);
}

int main() {
  using namespace afterhours;
  const float half_pi = 1.57079632679f;
  const float d = 0.70710678118f;

  check(near(dot(Vec3{1, 2, 3}, Vec3{4, 5, 6}), 32.f), "dot");
  check(near3(cross(Vec3{1, 0, 0}, Vec3{0, 1, 0}), {0, 0, 1}), "cross x*y=z");
  check(near3(cross(Vec3{0, 1, 0}, Vec3{1, 0, 0}), {0, 0, -1}), "cross order");
  check(near3(lerp(Vec3{0, 0, 0}, Vec3{2, 4, 6}, 0.5f), {1, 2, 3}), "lerp midpoint");
  check(near3(Vec3{1, 2, 3} + Vec3{4, 5, 6}, {5, 7, 9}), "Vec3 raylib add");
  check(near3(Vec3{1, 2, 3} * 2.f, {2, 4, 6}), "Vec3 raylib scale");

  check(near3(rotate(quat_axis_angle({0, 0, 1}, half_pi), {1, 0, 0}), {0, 1, 0}),
        "axis-angle rotate z90");
  check(near3(rotate(quat_axis_angle({0, 0, 5}, half_pi), {1, 0, 0}), {0, 1, 0}),
        "axis-angle normalizes axis");
  check(near3(rotate(quat_identity(), {1, 2, 3}), {1, 2, 3}), "identity rotate");

  {
    Quat a = quat_identity(), b = quat_axis_angle({0, 0, 1}, half_pi);
    check(near3(rotate(slerp(a, b, 0.f), {1, 0, 0}), {1, 0, 0}), "slerp t=0");
    check(near3(rotate(slerp(a, b, 1.f), {1, 0, 0}), {0, 1, 0}), "slerp t=1");
    check(near3(rotate(slerp(a, b, 0.5f), {1, 0, 0}), {d, d, 0}), "slerp midpoint");
  }
  {
    Quat z90 = quat_axis_angle({0, 0, 1}, half_pi);
    Quat x90 = quat_axis_angle({1, 0, 0}, half_pi);
    // multiply(z90, x90) applies x90 first, then z90: (0,1,0) -> (0,0,1) -> (0,0,1)
    check(near3(rotate(multiply(z90, x90), {0, 1, 0}), {0, 0, 1}), "quat multiply order a");
    // multiply(x90, z90) applies z90 first: (0,1,0) -> (-1,0,0) -> (-1,0,0)
    check(near3(rotate(multiply(x90, z90), {0, 1, 0}), {-1, 0, 0}), "quat multiply order b");
    check(near3(rotate(multiply(z90, z90), {1, 0, 0}), {-1, 0, 0}), "quat multiply composes");
  }
  {
    Mat4 t = to_mat4(TRS{{1, 2, 3}, quat_identity(), {2, 2, 2}});
    check(near3(translation_of(t), {1, 2, 3}), "TRS translation");
    check(near3(transform_point(t, {1, 0, 0}), {3, 2, 3}), "TRS scale+translate");
    Mat4 r = to_mat4(TRS{{0, 0, 0}, quat_axis_angle({0, 0, 1}, half_pi), {1, 1, 1}});
    check(near3(transform_point(r, {1, 0, 0}), {0, 1, 0}), "TRS rotate");
  }
  {
    Mat4 a = to_mat4(TRS{{1, 0, 0}, quat_axis_angle({0, 0, 1}, half_pi), {1, 1, 1}});
    Mat4 b = to_mat4(TRS{{0, 1, 0}, quat_identity(), {2, 1, 1}});
    check(near_mat(multiply(mat4_identity(), a), a), "Mat4 identity left");
    check(near_mat(multiply(a, mat4_identity()), a), "Mat4 identity right");
    check(near_mat(multiply(multiply(a, b), a), multiply(a, multiply(b, a))),
          "Mat4 multiply associativity");
    check(near3(transform_point(multiply(a, b), {0, 0, 0}),
                transform_point(a, transform_point(b, {0, 0, 0}))),
          "Mat4 multiply applies right first");
  }

  {
    // Agreement with raw raymath (the delegated backend, called directly).
    const Vec3 a{1, -2, 3}, b{-4, 5, 6};
    raylib::Vector3 rc = raylib::Vector3CrossProduct(a, b);
    check(near3(cross(a, b), {rc.x, rc.y, rc.z}), "cross agrees with raymath");
    check(near(dot(a, b), raylib::Vector3DotProduct(a, b)), "dot agrees with raymath");
    Quat q1 = quat_axis_angle({0, 0, 1}, half_pi), q2 = quat_axis_angle({1, 0, 0}, half_pi);
    raylib::Vector3 rr = raylib::Vector3RotateByQuaternion(
        {0, 1, 0}, raylib::QuaternionMultiply(q1, q2));
    // multiply() normalizes where raymath does not; both rotate the same.
    check(near3(rotate(multiply(q1, q2), {0, 1, 0}), {rr.x, rr.y, rr.z}), "quat multiply agrees with raymath");
    Mat4 m1 = to_mat4(TRS{{1, 2, 3}, q1, {2, 1, 1}}), m2 = to_mat4(TRS{{-1, 0, 2}, q2, {1, 3, 1}});
    // raymath applies its left operand first, so the raw equivalent of multiply(m1, m2) swaps.
    raylib::Matrix rm = raylib::MatrixMultiply(m2, m1);
    Mat4 prod = multiply(m1, m2);
    check(near(prod.m0, rm.m0) && near(prod.m5, rm.m5) && near(prod.m12, rm.m12) &&
          near(prod.m15, rm.m15), "Mat4 multiply agrees with raymath (swapped)");
    check(near3(rotate(quat_axis_angle({0, 0, 0}, half_pi), {1, 0, 0}), {1, 0, 0}),
          "zero axis is identity");
    Quat tiny = slerp(quat_identity(), quat_axis_angle({0, 0, 1}, 0.02f), 0.5f);
    check(near3(rotate(tiny, {1, 0, 0}), {std::cos(0.01f), std::sin(0.01f), 0}),
          "slerp near-parallel midpoint");
  }
  printf("%s: %d/%d checks passed\n", checks_passed == checks_run ? "PASS" : "FAIL",
         checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
