#include <afterhours/src/skeletal.h>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

static int checks_run = 0, checks_passed = 0;
static void check(bool c, const std::string &w) {
  checks_run++;
  if (c) checks_passed++; else fprintf(stderr, "  FAIL: %s\n", w.c_str());
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
static bool near3(afterhours::skeletal::Vec3 a, afterhours::skeletal::Vec3 b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

int main() {
  using namespace afterhours::skeletal;
  const float half_pi = 1.57079632679f;

  // FK chain: three joints, each +1 x locally -> world x = 1, 2, 3 exactly.
  {
    Skeleton sk; sk.parent = {-1, 0, 1};
    Pose pose = bind_pose(3);
    pose.local[0].pos = {1, 0, 0}; pose.local[1].pos = {1, 0, 0};
    pose.local[2].pos = {1, 0, 0};
    std::vector<Mat4> world; compute_world(sk, pose, world);
    check(near3(translation_of(world[0]), {1, 0, 0}), "root world pos");
    check(near3(translation_of(world[1]), {2, 0, 0}), "child world pos");
    check(near3(translation_of(world[2]), {3, 0, 0}), "grandchild world pos");
    check(near3(transform_point(world[2], {0, 0, 0}), {3, 0, 0}), "origin maps");
  }
  // FK rotation: root rotated 90deg about Z sends child offset to +y.
  {
    Skeleton sk; sk.parent = {-1, 0};
    Pose pose = bind_pose(2);
    pose.local[0].rot = Quat::axis_angle({0, 0, 1}, half_pi);
    pose.local[1].pos = {1, 0, 0};
    std::vector<Mat4> world; compute_world(sk, pose, world);
    check(near3(translation_of(world[1]), {0, 1, 0}), "rotated child world pos");
    check(near3(rotate(pose.local[0].rot, {1, 0, 0}), {0, 1, 0}), "quat rotate");
  }
  // Slerp midpoint: identity -> 180deg about Z, half way is 90deg.
  {
    Quat mid = slerp(Quat::identity(), Quat::axis_angle({0, 0, 1}, half_pi),
                     0.5f);
    const float d = 0.70710678118f;  // 45deg: half of the 90deg endpoint
    check(near3(rotate(mid, {1, 0, 0}), {d, d, 0}), "slerp midpoint");
    check(near3(rotate(slerp(Quat::identity(), Quat::axis_angle({0, 0, 1}, half_pi), 0.f), {1, 0, 0}), {1, 0, 0}), "slerp t=0");
  }
  // Skinning: world T(2,0,0) * inverse-bind T(-1,0,0) = T(1,0,0).
  {
    Skeleton sk; sk.parent = {-1};
    TRS bind; bind.pos = {1, 0, 0};
    sk.inverse_bind = {to_mat4(TRS{{-1, 0, 0}, {}, {1, 1, 1}})};
    Pose pose = bind_pose(1); pose.local[0].pos = {2, 0, 0};
    std::vector<Mat4> world, skin;
    compute_world(sk, pose, world); compute_skinning(sk, world, skin);
    check(near3(transform_point(skin[0], {1, 2, 3}), {2, 2, 3}), "skin = world*invbind");
    check(near3(translation_of(skin[0]), {1, 0, 0}), "skin translation");
  }
  // Clip sampling: pos lerps, rot slerps, untracked joints keep base, clamp ends.
  {
    Clip clip; clip.duration = 1.f; clip.joints.resize(2);
    clip.joints[0].pos = {{0.f, 1.f}, {{0, 0, 0}, {2, 0, 0}}};
    clip.joints[0].rot = {{0.f, 1.f},
                          {Quat::identity(), Quat::axis_angle({0, 0, 1}, half_pi)}};
    Pose pose = bind_pose(2); pose.local[1].pos = {9, 9, 9};
    sample(clip, 0.5f, pose);
    check(near3(pose.local[0].pos, {1, 0, 0}), "clip pos midpoint");
    const float d = 0.70710678118f;
    check(near3(rotate(pose.local[0].rot, {1, 0, 0}), {d, d, 0}), "clip rot midpoint");
    check(near3(pose.local[1].pos, {9, 9, 9}), "untracked joint keeps base");
    Pose end = bind_pose(2); sample(clip, 5.f, end);
    check(near3(end.local[0].pos, {2, 0, 0}), "clip clamps past last key");
  }
  printf("%s: %d/%d checks passed\n", checks_passed == checks_run ? "PASS" : "FAIL",
         checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
