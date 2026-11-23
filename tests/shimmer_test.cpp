#define AFTER_HOURS_USE_NONE

#include <afterhours/src/plugins/ui/shimmer.h>

#include <cmath>
#include <cstdio>
#include <string>

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

static float brightest_x(const presets::Shimmer &s, RectangleType r, float t) {
  int count = 0;
  const auto strips = presets::shimmer_strips(s, r, t, count);
  float best = -1.f, bx = -1.f;
  for (int i = 0; i < count; ++i) {
    const float v = strips[i].color.r;
    if (v > best) {
      best = v;
      bx = strips[i].rect.x;
    }
  }
  return bx;
}

int main() {
  printf("Running shimmer tests...\n\n");
  presets::Shimmer s;
  s.period = 2.f;
  s.base = {40, 40, 40, 255};
  s.highlight = {240, 240, 240, 255};
  const RectangleType r{0.f, 0.f, 200.f, 40.f};

  int count = 0;
  const auto strips = presets::shimmer_strips(s, r, 0.f, count);
  check(count == 16, "default strip count fills the rect");
  check(std::abs(strips[0].rect.x - 0.f) < 1e-3f &&
            std::abs(strips[15].rect.x + strips[15].rect.width - 200.f) < 1e-3f,
        "strips tile the rect edge to edge");

  const float x0 = brightest_x(s, r, 0.f);
  const float x1 = brightest_x(s, r, 1.f);
  check(x1 > x0, "shimmer offset animates: brightest strip moves with t");
  check(brightest_x(s, r, 2.f) == x0, "shimmer wraps at one period");

  int c2 = 0;
  const auto mid = presets::shimmer_strips(s, r, 1.f, c2);
  float peak = 0.f;
  for (int i = 0; i < c2; ++i)
    peak = std::max(peak, static_cast<float>(mid[i].color.r));
  check(peak > 180.f, "band peak approaches the highlight colour");

  presets::Shimmer off = s;
  off.enabled = false;
  int c3 = 0, c4 = 0;
  const auto d0 = presets::shimmer_strips(off, r, 0.f, c3);
  const auto d9 = presets::shimmer_strips(off, r, 9.f, c4);
  check(c3 == 1 && c4 == 1 && d0[0].color.r == 40 && d9[0].color.r == 40 &&
            d0[0].rect.width == 200.f,
        "disabled shimmer is one static base strip");

  int c5 = 0;
  presets::shimmer_strips(s, {0.f, 0.f, 0.f, 40.f}, 0.f, c5);
  check(c5 == 0, "zero-width rect yields no strips");

  capture::enable();
  capture::clear();
  draw_shimmer(r, 1.f, s);
  check(capture::calls().size() == 16, "draw_shimmer draws one rect per strip");
  capture::disable();

  printf("%d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
