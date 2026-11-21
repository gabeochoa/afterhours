#include <afterhours/src/plugins/collision3d.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours::collision3d;

static int checks = 0, failures = 0;
static void check(bool c, const std::string &w) {
  ++checks;
  if (!c) { ++failures; std::printf("  FAIL: %s\n", w.c_str()); }
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
static bool near3(Vec3 a, Vec3 b, float eps = 1e-3f) {
  return near(a.x, b.x, eps) && near(a.y, b.y, eps) && near(a.z, b.z, eps);
}

int main() {
  {
    World w;
    w.add(AABB{{2, -1, -1}, {3, 1, 1}, 7});
    w.add(Sphere{{0, 0, 5}, 1.f, 9});
    auto hit = raycast(w, {0, 0, 0}, {1, 0, 0});
    check(hit && near(hit->t, 2.f) && hit->material == 7 && hit->kind == Kind::AABB, "ray hits aabb front face + material");
    check(hit && near3(hit->normal, {-1, 0, 0}), "aabb hit normal faces ray");
    auto miss = raycast(w, {0, 5, 0}, {1, 0, 0});
    check(!miss, "ray above boxes misses");
    auto sh = raycast(w, {0, 0, 0}, {0, 0, 1});
    check(sh && near(sh->t, 4.f) && sh->material == 9 && sh->kind == Kind::Sphere, "ray hits sphere + material");
    auto capped = raycast(w, {0, 0, 0}, {1, 0, 0}, 1.f);
    check(!capped, "max_dist caps the ray");
  }
  {
    World w;
    w.add(Capsule{{5, 0, 0}, 0.5f, 1.f, 3});
    auto side = raycast(w, {0, 0, 0}, {1, 0, 0});
    check(side && near(side->t, 4.5f) && side->material == 3 && side->kind == Kind::Capsule, "ray hits capsule side");
    auto cap = raycast(w, {5, 5, 0}, {0, -1, 0});
    check(cap && near(cap->t, 3.5f) && cap->kind == Kind::Capsule, "ray hits capsule top cap");
    auto miss = raycast(w, {0, 3, 0}, {1, 0, 0});
    check(!miss, "ray over capsule misses");
  }
  {
    World w;
    w.add(Heightfield{[](float x, float) { return 0.1f * x; }, -10, 10, -10, 10, 5});
    auto down = raycast(w, {2, 5, 0}, {0, -1, 0});
    check(down && near(down->point.y, 0.2f, 0.05f) && down->material == 5 && down->kind == Kind::Heightfield, "ray hits heightfield + material");
    check(down && down->normal.y > 0.9f, "heightfield normal points up");
    auto up = raycast(w, {2, 5, 0}, {0, 1, 0});
    check(!up, "ray away from heightfield misses");
    auto g = ground_height(w, 3.f, 0.f);
    check(g && near(*g, 0.3f), "ground_height samples the field");
  }

  {
    World w;
    w.add(AABB{{0, 0, 0}, {1, 1, 1}, 1});
    w.add(AABB{{5, 5, 5}, {6, 6, 6}, 2});
    w.add(Sphere{{1.4f, 0.5f, 0.5f}, 0.5f, 3});
    w.add(Capsule{{10, 0, 0}, 0.5f, 1.f, 4});
    auto box = overlap_aabb(w, {-1, -1, -1}, {2, 2, 2});
    check(box.size() == 2 && box[0].material == 1 && box[1].material == 3, "overlap_aabb returns exact set");
    auto sph = overlap_sphere(w, {0.5f, 0.5f, 0.5f}, 1.f);
    check(sph.size() == 2, "overlap_sphere returns exact set");
    auto far = overlap_sphere(w, {100, 100, 100}, 1.f);
    check(far.empty(), "overlap far away is empty");
  }

  {
    World w;
    w.add(AABB{{0, 0, 0}, {2, 2, 2}, 1});
    Vec3 c{2.2f, 1, 1};
    Vec3 push = push_out_sphere(w, c, 0.5f);
    Vec3 resolved = c + push;
    check(near(resolved.x, 2.5f), "push-out vs aabb lands on surface");
    check(near3(push_out_sphere(w, resolved, 0.5f), {0, 0, 0}), "resolved sphere no longer penetrates aabb");
    Vec3 inside = push_out_sphere(w, {1.9f, 1, 1}, 0.5f);
    check(near((Vec3{1.9f, 1, 1} + inside).x, 2.5f), "push-out from inside aabb exits nearest face");
  }
  {
    World w;
    w.add(Sphere{{0, 0, 0}, 1.f, 1});
    Vec3 push = push_out_sphere(w, {1.2f, 0, 0}, 0.5f);
    check(near(1.2f + push.x, 1.5f), "push-out vs sphere lands on surface");
  }
  {
    World w;
    w.add(Heightfield{[](float, float) { return 2.f; }, -10, 10, -10, 10, 1});
    Vec3 push = push_out_sphere(w, {0, 1.8f, 0}, 0.5f);
    check(near3(push, {0, 0.7f, 0}), "push-out vs heightfield lifts sphere to ground+radius");
    check(near3(push_out_sphere(w, {0, 2.5f, 0}, 0.5f), {0, 0, 0}), "sphere resting on heightfield needs no push");
  }

  std::printf("collision3d_test: %d/%d checks passed\n", checks - failures, checks);
  return failures ? 1 : 0;
}
