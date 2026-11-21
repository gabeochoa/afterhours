#include <afterhours/src/physics2d.h>

#include <cmath>
#include <cstdio>

using namespace afterhours::physics2d;

static int run = 0, passed = 0;
static void check(bool c, const char *e, int l) {
  run++;
  if (c) passed++;
  else fprintf(stderr, "  FAIL: %s (line %d)\n", e, l);
}
#define CHECK(e) check((e), #e, __LINE__)
#define NEAR(a, b) (std::fabs((a) - (b)) < 0.01f)

int main() {
  // approach / horizontal velocity
  CHECK(NEAR(approach(0, 10, 3), 3));
  CHECK(NEAR(approach(9, 10, 3), 10));
  HorizontalParams hp;
  float v = 0;
  for (int i = 0; i < 60; i++) v = horizontal_velocity(v, 1, 1.f / 60, hp, true);
  CHECK(v > 0 && v <= hp.max_speed && NEAR(v, hp.max_speed));
  v = horizontal_velocity(100, -1, 0.01f, hp, true); // turn boost: faster reversal
  CHECK(v < 100 - hp.accel * 0.01f);
  v = hp.max_speed;
  for (int i = 0; i < 600 && v != 0; i++) v = horizontal_velocity(v, 0, 1.f / 60, hp, true);
  CHECK(v == 0); // stop snap, no drift

  // time_until_height: y-up, gravity -100, tossed up at 50 from 0 -> back at 0 at t=1
  auto th = time_until_height(0, 50, 0, -100);
  CHECK(th && NEAR(*th, 0.f)); // earliest is launch instant
  auto th2 = time_until_height(0, 50, 12, -100); // 50t-50t^2=12 -> earliest t=0.4
  CHECK(th2 && NEAR(*th2, 0.4f));
  CHECK(!time_until_height(0, 10, 1000, -100).has_value()); // never that high

  // intercept: stationary target, speed 10, distance 100 -> t=10
  auto it = intercept_time({0, 0}, {100, 0}, {0, 0}, 10);
  CHECK(it && NEAR(*it, 10));
  // moving target: target at (100,0) moving +5x, projectile 10 -> t=20, point 200
  auto ip = intercept_point({0, 0}, {100, 0}, {5, 0}, 10);
  CHECK(ip && NEAR(ip->x, 200) && NEAR(ip->y, 0));
  CHECK(!intercept_time({0, 0}, {100, 0}, {20, 0}, 10).has_value()); // outruns

  // arrival decelerates to target and settles (no oscillation)
  Vec2 pos{0, 0}, vel{0, 0};
  Vec2 target{100, 0};
  for (int i = 0; i < 60 * 10; i++) {
    vel = arrive_velocity(pos, vel, target, 100, 50, 500, 1.f / 60, 1.f);
    pos = pos + vel * (1.f / 60);
  }
  CHECK(length(target - pos) < 2.f);
  CHECK(length(vel) < 5.f);

  // swept ellipse: head-on hit, miss, and tunneling (fast, thin target)
  Ellipse fixed{{100, 0}, {20, 10}};
  auto hit = swept_ellipse({{0, 0}, {10, 10}}, {100, 0}, fixed);
  CHECK(hit && hit->t > 0 && hit->t < 1 && hit->normal.x < 0);
  CHECK(!swept_ellipse({{0, 50}, {10, 10}}, {100, 0}, fixed).has_value());
  auto tun = swept_ellipse({{0, 0}, {5, 5}}, {300, 0}, {{150, 0}, {5, 20}});
  CHECK(tun.has_value()); // swept catches what endpoint overlap would miss
  CHECK(!ellipses_overlap({{0, 0}, {5, 5}}, {{150, 0}, {5, 20}}));
  // overlap + separation pushes out
  Ellipse a{{0, 0}, {10, 10}}, b{{15, 0}, {10, 10}};
  CHECK(ellipses_overlap(a, b));
  Vec2 sep = ellipse_separation(a, b);
  a.center = a.center + sep;
  CHECK(!ellipses_overlap(a, b) || NEAR(ellipse_metric(a.center - b.center, {20, 20}), 1.f));

  printf("%d/%d checks passed\n", passed, run);
  return passed == run ? 0 : 1;
}
