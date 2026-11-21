#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/blob_shadow.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;
static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) { ++failures; std::printf("  FAIL: %s\n", what.c_str()); }
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }
static float flat(float, float) { return 2.f; }

int main() {
  BlobShadowConfig cfg; // base .35, max_height 8, growth .6, offset .02
  { // on the ground: footprint size, full alpha, centre at ground+offset
    auto b = compute_blob_shadow({5.f, 2.f, -3.f}, 0.6f, 0.4f, flat, cfg);
    check(near(b.center.x, 5.f) && near(b.center.y, 2.02f) && near(b.center.z, -3.f), "ground centre");
    check(near(b.radius_x, 0.6f) && near(b.radius_z, 0.4f), "ground radii = footprint");
    check(near(b.alpha, 0.35f) && near(b.height, 0.f) && b.visible(), "ground alpha/visible");
  }
  { // halfway up: alpha halved, radii +30%
    auto b = compute_blob_shadow({0.f, 6.f, 0.f}, 1.f, flat, cfg);
    check(near(b.height, 4.f) && near(b.alpha, 0.175f), "mid height alpha");
    check(near(b.radius_x, 1.3f) && near(b.radius_z, 1.3f), "mid height radii spread");
  }
  { // at/above max height: invisible
    auto b = compute_blob_shadow({0.f, 10.f, 0.f}, 1.f, flat, cfg);
    auto c = compute_blob_shadow({0.f, 99.f, 0.f}, 1.f, flat, cfg);
    check(near(b.alpha, 0.f) && !b.visible(), "at max height invisible");
    check(near(c.alpha, 0.f) && near(c.radius_x, 1.6f), "above max height clamps");
  }
  { // below ground clamps to full strength at the ground
    auto b = compute_blob_shadow({1.f, -5.f, 1.f}, 0.5f, flat, cfg);
    check(near(b.height, 0.f) && near(b.center.y, 2.02f) && near(b.alpha, 0.35f), "below ground clamp");
  }
  { // sloped ground sampler is honoured at the object's x/z
    auto slope = [](float x, float z) { return 0.5f * x + z; };
    auto b = compute_blob_shadow({4.f, 10.f, 2.f}, 1.f, slope, cfg);
    check(near(b.center.y, 4.02f) && near(b.height, 6.f), "sampler ground height");
  }
  { // degenerate inputs never go negative/visible
    auto b = compute_blob_shadow({0.f, 2.f, 0.f}, -1.f, 0.f, flat, cfg);
    check(!b.visible() && near(b.radius_x, 0.f), "negative/zero footprint invisible");
    BlobShadowConfig zero = cfg; zero.base_alpha = 0.f;
    check(!compute_blob_shadow({0.f, 2.f, 0.f}, 1.f, flat, zero).visible(), "zero base alpha invisible");
  }
  draw_blob_shadow(compute_blob_shadow({0.f, 2.f, 0.f}, 1.f, flat, cfg), cfg); // none backend: no crash
  std::printf("blob_shadow_test: %d/%d checks passed\n", checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
