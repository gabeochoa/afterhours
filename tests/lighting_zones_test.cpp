#include <afterhours/src/lighting_zones.h>

#include <cmath>
#include <cstdio>

using namespace afterhours;

static int run = 0, passed = 0;
static bool near(float a, float b) { return std::fabs(a - b) < 1e-5f; }
static void check(bool c, const char *w) {
  run++;
  if (c) passed++;
  else fprintf(stderr, "  FAIL: %s\n", w);
}

int main() {
  LightingZoneSet set;
  set.default_lighting = {{0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, 0.f, 100.f, 1.f};
  LightingZone a;
  a.name = "west";
  a.min = {0.f, 0.f, 0.f};
  a.max = {10.f, 10.f, 10.f};
  a.blend_width = 2.f;
  a.lighting = {{1.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, 0.f, 50.f, 2.f};
  LightingZone b;
  b.name = "east";
  b.min = {10.f, 0.f, 0.f};
  b.max = {20.f, 10.f, 10.f};
  b.blend_width = 2.f;
  b.lighting = {{0.f, 0.f, 1.f}, {0.f, 0.f, 0.f}, 10.f, 60.f, 4.f};
  set.add_zone(a);
  set.add_zone(b);

  // Deep inside one zone: exact.
  const ZoneLighting west = set.sample({5.f, 5.f, 5.f});
  check(near(west.ambient.r, 1.f) && near(west.ambient.b, 0.f), "inside west ambient");
  check(near(west.exposure, 2.f) && near(west.fog_far, 50.f), "inside west exposure/fog");

  // Midpoint of the blend band on the shared face: average of both zones.
  const ZoneLighting mid = set.sample({10.f, 5.f, 5.f});
  check(near(mid.ambient.r, 0.5f) && near(mid.ambient.b, 0.5f), "shared face averages ambient");
  check(near(mid.exposure, 3.f) && near(mid.fog_near, 5.f), "shared face averages scalars");

  // Outside all zones: default.
  const ZoneLighting out = set.sample({-100.f, 5.f, 5.f});
  check(near(out.ambient.r, 0.f) && near(out.exposure, 1.f) && near(out.fog_far, 100.f),
        "outside is default");

  printf("lighting_zones_test: %d/%d passed\n", passed, run);
  return passed == run ? 0 : 1;
}
