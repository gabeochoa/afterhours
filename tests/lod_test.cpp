// lod_test.cpp
// Lod::update under the none backend: levels change at threshold * (1 +/-
// margin), not at the threshold, so a copy hovering on the line holds.

#define FMT_HEADER_ONLY
#define AFTER_HOURS_USE_NONE
#include <fmt/format.h>

#include <afterhours/ah.h>
#include <afterhours/src/plugins/lod.h>

#include <cstdio>
#include <string>

using namespace afterhours;

static int checks = 0;
static int failures = 0;

static void check(bool cond, const std::string &what) {
  ++checks;
  if (!cond) {
    ++failures;
    std::printf("  FAIL: %s\n", what.c_str());
  }
}

int main() {
  // cartographer's numbers: out at 35, back at 25.
  {
    Lod lod;
    const float m = 1.f / 6.f;
    check(lod.update(30.f, {30.f}, m) == 0, "on the threshold stays near");
    check(lod.update(34.f, {30.f}, m) == 0, "inside the margin stays near");
    check(lod.update(36.f, {30.f}, m) == 1, "past the margin goes far");
    check(lod.update(30.f, {30.f}, m) == 1, "back on the threshold stays far");
    check(lod.update(26.f, {30.f}, m) == 1, "inside the margin stays far");
    check(lod.update(24.f, {30.f}, m) == 0, "past the margin comes back");
  }

  // Jitter across the threshold every frame never switches.
  {
    Lod lod;
    int switches = 0, last = 0;
    for (int i = 0; i < 100; i++) {
      int level = lod.update(i % 2 ? 31.f : 29.f, {30.f});
      if (level != last) switches++;
      last = level;
    }
    check(switches == 0, "jitter on the line never switches");
  }

  // Several levels, and jumping straight to the far end.
  {
    Lod lod;
    check(lod.update(500.f, {10.f, 50.f, 100.f}) == 3, "far copy jumps to the last level");
    check(lod.update(70.f, {10.f, 50.f, 100.f}) == 2, "steps in one band");
    check(lod.update(1.f, {10.f, 50.f, 100.f}) == 0, "close copy jumps to level 0");
  }

  // Thresholds shrinking under an existing level clamp instead of indexing
  // past the end.
  {
    Lod lod;
    lod.update(500.f, {10.f, 50.f, 100.f});
    check(lod.update(500.f, {10.f}) == 1, "fewer thresholds clamp the level");
  }

  // update_sq agrees with update on squared input, margins included.
  {
    Lod a, b;
    const float dists[] = {30.f, 34.f, 36.f, 30.f, 26.f, 24.f, 500.f, 70.f, 1.f};
    bool same = true;
    for (float d : dists) {
      if (a.update(d, {10.f, 30.f, 100.f}, 1.f / 6.f) !=
          b.update_sq(d * d, {10.f, 30.f, 100.f}, 1.f / 6.f))
        same = false;
    }
    check(same, "update_sq matches update on squared distances");
    Lod c;
    check(c.update_sq(36.f * 36.f, {30.f}, 1.f / 6.f) == 1, "update_sq goes far past the margin");
    check(c.update_sq(26.f * 26.f, {30.f}, 1.f / 6.f) == 1, "update_sq holds inside the margin");
    check(c.update_sq(24.f * 24.f, {30.f}, 1.f / 6.f) == 0, "update_sq comes back past the margin");
  }

  std::printf("lod_test: %d/%d checks passed\n", checks - failures, checks);
  return failures > 0 ? 1 : 0;
}
