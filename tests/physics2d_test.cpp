#include <afterhours/src/plugins/physics2d.h>

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
  CHECK(NEAR(approach(0, 10, 3), 3));
  CHECK(NEAR(approach(9, 10, 3), 10));
  HorizontalParams hp;
  float v = 0;
  for (int i = 0; i < 60; i++) v = horizontal_velocity(v, 1, 1.f / 60, hp, true);
  CHECK(v > 0 && v <= hp.max_speed && NEAR(v, hp.max_speed));
  v = horizontal_velocity(100, -1, 0.01f, hp, true);
  CHECK(v < 100 - hp.accel * 0.01f);
  v = hp.max_speed;
  for (int i = 0; i < 600 && v != 0; i++) v = horizontal_velocity(v, 0, 1.f / 60, hp, true);
  CHECK(v == 0);

  auto th = time_until_height(0, 50, 0, -100);
  CHECK(th && NEAR(*th, 0.f));
  auto th2 = time_until_height(0, 50, 12, -100);
  CHECK(th2 && NEAR(*th2, 0.4f));
  CHECK(!time_until_height(0, 10, 1000, -100).has_value());

  auto it = intercept_time({0, 0}, {100, 0}, {0, 0}, 10);
  CHECK(it && NEAR(*it, 10));
  auto ip = intercept_point({0, 0}, {100, 0}, {5, 0}, 10);
  CHECK(ip && NEAR(ip->x, 200) && NEAR(ip->y, 0));
  CHECK(!intercept_time({0, 0}, {100, 0}, {20, 0}, 10).has_value());

  Vector2Type pos{0, 0}, vel{0, 0};
  Vector2Type target{100, 0};
  for (int i = 0; i < 60 * 10; i++) {
    vel = arrive_velocity(pos, vel, target, 100, 50, 500, 1.f / 60, 1.f);
    pos = {pos.x + vel.x * (1.f / 60), pos.y + vel.y * (1.f / 60)};
  }
  CHECK(std::sqrt(afterhours::distance_sq(target, pos)) < 2.f);
  CHECK(std::sqrt(vel.x * vel.x + vel.y * vel.y) < 5.f);

  Ellipse fixed{{100, 0}, {20, 10}};
  auto hit = swept_ellipse({{0, 0}, {10, 10}}, {100, 0}, fixed);
  CHECK(hit && hit->t > 0 && hit->t < 1 && hit->normal.x < 0);
  CHECK(!swept_ellipse({{0, 50}, {10, 10}}, {100, 0}, fixed).has_value());
  auto tun = swept_ellipse({{0, 0}, {5, 5}}, {300, 0}, {{150, 0}, {5, 20}});
  CHECK(tun.has_value());
  CHECK(!ellipses_overlap({{0, 0}, {5, 5}}, {{150, 0}, {5, 20}}));
  Ellipse a{{0, 0}, {10, 10}}, b{{15, 0}, {10, 10}};
  CHECK(ellipses_overlap(a, b));
  Vector2Type sep = ellipse_separation(a, b);
  a.center = {a.center.x + sep.x, a.center.y + sep.y};
  Vector2Type delta{a.center.x - b.center.x, a.center.y - b.center.y};
  CHECK(!ellipses_overlap(a, b) || NEAR(ellipse_metric(delta, {20, 20}), 1.f));

  printf("%d/%d checks passed\n", passed, run);
  return passed == run ? 0 : 1;
}
