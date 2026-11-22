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

  {
    // Agreement with raw raymath (the delegated backend, called directly).
    const Vec3 a{1, -2, 3}, b{-4, 5, 6};
    raylib::Vector3 ra{a.x, a.y, a.z}, rb{b.x, b.y, b.z};
    raylib::Vector3 rc = raylib::Vector3CrossProduct(ra, rb);
    check(near3(cross(a, b), {rc.x, rc.y, rc.z}), "cross agrees with raymath");
    check(near(dot(a, b), raylib::Vector3DotProduct(ra, rb)), "dot agrees with raymath");
    Quat q1 = Quat::axis_angle({0, 0, 1}, half_pi), q2 = Quat::axis_angle({1, 0, 0}, half_pi);
    raylib::Vector3 rr = raylib::Vector3RotateByQuaternion(
        {0, 1, 0}, raylib::QuaternionMultiply({q1.x, q1.y, q1.z, q1.w}, {q2.x, q2.y, q2.z, q2.w}));
    // Our operator* normalizes where raymath does not; both rotate the same.
    check(near3(rotate(q1 * q2, {0, 1, 0}), {rr.x, rr.y, rr.z}), "quat multiply agrees with raymath");
    Mat4 m1 = to_mat4(TRS{{1, 2, 3}, q1, {2, 1, 1}}), m2 = to_mat4(TRS{{-1, 0, 2}, q2, {1, 3, 1}});
    auto to_rm = [](const Mat4 &m) {
      raylib::Matrix r; r.m0=m.m[0]; r.m1=m.m[1]; r.m2=m.m[2]; r.m3=m.m[3];
      r.m4=m.m[4]; r.m5=m.m[5]; r.m6=m.m[6]; r.m7=m.m[7]; r.m8=m.m[8]; r.m9=m.m[9];
      r.m10=m.m[10]; r.m11=m.m[11]; r.m12=m.m[12]; r.m13=m.m[13]; r.m14=m.m[14]; r.m15=m.m[15]; return r;
    };
    // raymath applies its left operand first, so the raw equivalent of m1*m2 swaps.
    raylib::Matrix rm = raylib::MatrixMultiply(to_rm(m2), to_rm(m1));
    Mat4 prod = m1 * m2;
    check(near(prod.m[0], rm.m0) && near(prod.m[5], rm.m5) && near(prod.m[12], rm.m12) &&
          near(prod.m[15], rm.m15), "Mat4 multiply agrees with raymath (swapped)");
    check(near3(rotate(Quat::axis_angle({0, 0, 0}, half_pi), {1, 0, 0}), {1, 0, 0}),
          "zero axis is identity");
    Quat tiny = slerp(Quat::identity(), Quat::axis_angle({0, 0, 1}, 0.02f), 0.5f);
    check(near3(rotate(tiny, {1, 0, 0}), {std::cos(0.01f), std::sin(0.01f), 0}),
          "slerp near-parallel midpoint");
  }
  printf("%s: %d/%d checks passed\n", checks_passed == checks_run ? "PASS" : "FAIL",
         checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
